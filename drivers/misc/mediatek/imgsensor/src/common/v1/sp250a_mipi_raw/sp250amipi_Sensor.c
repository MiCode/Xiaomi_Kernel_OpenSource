// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*
 * Filename:
 * ---------
 *     SP250amipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 * Author:
 * --------
 *     Dream Yeh (HuanYu.Ye@transsion.com)
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "sp250amipi_Sensor.h"
/****************************Modify Following Strings for Debug****************************/
#define PFX "SP250a_camera_sensor"
#define LOG_1 LOG_INF("SP250a,MIPI 1LANE\n")
#define LOG_2 LOG_INF("preview 1280*960@30fps,420Mbps/lane; video 1280*960@30fps,420Mbps/lane; capture 5M@15fps,420Mbps/lane\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

#if defined(MT6765)
#define STEREO_CUSTOM1_30FPS 1
#else
#define STEREO_CUSTOM1_30FPS 0
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);




static imgsensor_info_struct imgsensor_info = {
	.sensor_id = SP250A_SENSOR_ID,

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	.checksum_value = 0x2593b889,
#else
	.checksum_value = 0xf77cdce9,        //checksum value for Camera Auto Test
#endif

	.pre = {
		.pclk = 42000000,              //record different mode's pclk
		.linelength = 1131,             //record different mode's linelength
		.framelength = 1229,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1600,        //record different mode's width of grabwindow
		.grabwindow_height = 1200,        //record different mode's height of grabwindow
		/*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*     following for GetDefaultFramerateByScenario()    */
		.mipi_pixel_rate = 42000000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 42000000,
		.linelength = 1131,
		.framelength = 1229,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 42000000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 42000000,
		.linelength = 1131,
		.framelength = 1229,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 42000000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 42000000,
		.linelength = 1131,
		.framelength = 1229,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 42000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 42000000,
		.linelength = 1131,
		.framelength = 1229,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 42000000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 84000000,
		.linelength = 1131,
		.framelength = 1229,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 84000000,
		.max_framerate = 300,
	},

#if STEREO_CUSTOM1_30FPS
	// 2 M * 30 fps
	.custom1 = {
		.pclk = 42000000,
		.linelength = 1131,
		.framelength = 1229,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 42000000,
		.max_framerate = 300,
	},
#else
	// 2 M * 24 fps
	.custom1 = {
		.pclk = 42000000,
		.linelength = 1131,
		.framelength = 1547,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,//4192,
		.grabwindow_height = 1200,//3104,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 42000000,
		.max_framerate = 240,
	},
#endif

	.margin = 4,
	.min_shutter = 7,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
	.frame_time_delay_frame = 1,    //The delay frame of setting frame length   //modify dualcam IDLHLYE-99 by wei.miao@reallytek.com 180629
	.ihdr_support = 0,      // 1 support; 0 not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 7,      //support sensor mode num

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

	.custom1_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#endif

	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_1_LANE,//mipi lane num
	.i2c_addr_table = {0x78, 0x7a, 0xff},
	.i2c_speed = 300, // i2c read/write speed
};


static imgsensor_struct imgsensor = {

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	.mirror = IMAGE_HV_MIRROR,
#else
	.mirror = IMAGE_NORMAL,
#endif

	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x46e,                   //current shutter   // Danbo ??
	.gain = 0x40,                      //current gain     // Danbo ??
	.dummy_pixel = 272,                   //current dummypixel
	.dummy_line = 32,                    //current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,        //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x78,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT
	imgsensor_winsize_info[6] = {
	{  1600, 1200,  0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,  0, 0, 1600, 1200}, // Preview 2112*1558
	{  1600, 1200,  0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,  0, 0, 1600, 1200}, // capture 4206*3128
	{  1600, 1200,  0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,  0, 0, 1600, 1200}, // video
	{  1600, 1200,  0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,  0, 0, 1600, 1200}, //hight speed video
	{  1600, 1200,  0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,  0, 0, 1600, 1200},
	{  1600, 1200,  0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,  0, 0, 1600, 1200},//custom1
}; // slim video


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = {(char)(addr & 0xFF)};

	//kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1,
		    imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = {(char)(addr & 0xFF), (char)(para & 0xFF)};

	//kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}
static kal_uint16 read_cmos_sensor0(kal_uint32 addr)
{
	return 0;
}

static void write_cmos_sensor0(kal_uint32 addr, kal_uint32 para)
{

}

static void set_dummy(void)
{
	kal_uint32 vb = 318;//24 fps --> vb = 318
	kal_uint32 basic_line =
		1229;  // 30 fps --->framelen = 1229, 24 fps -----> framelen = 1547 = 1229 + vb = 1229 + 318

	vb = imgsensor.frame_length - basic_line;

	//vb = (vb < 318) ? 318 : vb;//compatiable with 30fps and 24fps

	LOG_INF("imgsensor.frame_length %d, dummyline = %d, vb :%d\n",
		imgsensor.frame_length, imgsensor.dummy_line, vb);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x05, (vb >> 8) & 0xFF);
	write_cmos_sensor(0x06, vb & 0xFF);
	write_cmos_sensor(0x01, 0x01);

	//write_cmos_sensor(0xfd, 0x01);
	//write_cmos_sensor(0x05, (imgsensor.dummy_line >> 8) & 0xFF);
	//write_cmos_sensor(0x06, imgsensor.dummy_line & 0xFF);
	//write_cmos_sensor(0x01, 0x01);
}    /*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	write_cmos_sensor(0xfd, 0x00);
	return ((read_cmos_sensor(0x02) << 8) | read_cmos_sensor(0x03));
}
static void set_max_framerate(UINT16 framerate,
			      kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable = %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 /
		       imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length >
				  imgsensor.min_frame_length) ? frame_length :
				 imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
			       imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
	//imgsensor.dummy_line = 0;
	//else
	//imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
				       imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	set_dummy();
}    /*    set_max_framerate  */

static void write_shutter(kal_uint16 shutter)
{
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x03, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x04, shutter  & 0xFF);
	write_cmos_sensor(0x01, 0x01);
	LOG_INF("shutter =%d, framelength =%d\n", shutter,
		imgsensor.frame_length);
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
static void set_shutter_frame_length(kal_uint16 shutter,
				     kal_uint16 target_frame_length)
{

	//LOG_INF("shutter: %d, target_frame_length: %d", shutter, target_frame_length);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.dummy_line = (target_frame_length >
				imgsensor_info.custom1.framelength) ? (target_frame_length -
						imgsensor_info.custom1.framelength) : 0;
	imgsensor.frame_length = imgsensor_info.custom1.framelength +
				 imgsensor.dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	LOG_INF("shutter: %d, frame_length: %d, custom1.framelength: %d dummy_line: %d\n",
		shutter, imgsensor.frame_length,
		imgsensor_info.custom1.framelength, imgsensor.dummy_line);
	spin_unlock(&imgsensor_drv_lock);

	set_dummy();
	set_shutter(shutter);

	/*
	//static bool setframerate = false;
	kal_uint32 test_frame_length = 0;
	static kal_uint32 temp_fps = 0;
	if(cqcqdebug ==1)
	{
		LOG_INF("temp_fps:%d, cqcqvalue:%d", temp_fps, cqcqvalue);
		if(temp_fps != cqcqvalue)
		{
			temp_fps = cqcqvalue;
			test_frame_length = imgsensor.pclk / cqcqvalue * 10 / imgsensor.line_length;
			imgsensor.dummy_line = (test_frame_length > imgsensor_info.custom1.framelength) ? (test_frame_length - imgsensor_info.custom1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			LOG_INF("test_frame_length:%d, dummy_line:%d", test_frame_length, imgsensor.dummy_line);
			set_dummy();
		}

	}
	*/

}

/*
static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint16 reg_gain = 0x0000;

    reg_gain = ((gain / BASEGAIN) << 4) + ((gain % BASEGAIN) * 16 / BASEGAIN);
    reg_gain = reg_gain & 0xFFFF;

    return (kal_uint16)reg_gain;
}
*/
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
	kal_uint8  iReg;

	if (gain >= BASEGAIN && gain <= 15 * BASEGAIN) {
		iReg = 0x10 * gain /
		       BASEGAIN;      //change mtk gain base to aptina gain base

		if (iReg >= 0x80 && imgsensor.shutter >= 0xb8d) {
			write_cmos_sensor(0xfd, 0x02);
			write_cmos_sensor(0x1f, 0x00);
			write_cmos_sensor(0x20, 0x04);
			write_cmos_sensor(0x21, 0xb0);
			write_cmos_sensor(0x28, 0x03);
			write_cmos_sensor(0x29, 0x22);
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x1f = 0x%x\n",
				read_cmos_sensor(0x1f));
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x20 = 0x%x\n",
				read_cmos_sensor(0x20));
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x21 = 0x%x\n",
				read_cmos_sensor(0x21));
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x28 = 0x%x\n",
				read_cmos_sensor(0x28));
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x29 = 0x%x\n",
				read_cmos_sensor(0x29));
		} else {
			write_cmos_sensor(0xfd, 0x02);
			write_cmos_sensor(0x1f, 0x02);
			write_cmos_sensor(0x20, 0x00);
			write_cmos_sensor(0x21, 0x02);
			write_cmos_sensor(0x28, 0x00);
			write_cmos_sensor(0x29, 0x03);
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x1f = 0x%x\n",
				read_cmos_sensor(0x1f));
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x20 = 0x%x\n",
				read_cmos_sensor(0x20));
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x21 = 0x%x\n",
				read_cmos_sensor(0x21));
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x28 = 0x%x\n",
				read_cmos_sensor(0x28));
			LOG_INF("SP250aMIPI_SetGain 1111111111--0x29 = 0x%x\n",
				read_cmos_sensor(0x29));
		}

		if (iReg <= 0x10) {
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, 0x10);//0x23
			write_cmos_sensor(0x01, 0x01);
			LOG_INF("SP250aMIPI_SetGain = 16");
		} else if (iReg >= 0xa0) { //gpw
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, 0xa0);
			write_cmos_sensor(0x01, 0x01);
			LOG_INF("SP250aMIPI_SetGain = 160");
		} else {
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, (kal_uint8)iReg);
			write_cmos_sensor(0x01, 0x01);
			LOG_INF("SP250aMIPI_SetGain = %d", iReg);
		}
	} else
		LOG_INF("error gain setting");

	return gain;
}    /*    set_gain  */

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x3f, 0x00);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x3f, 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x3f, 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x3f, 0x03);
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
	}
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
}    /*    night_mode    */

static void sensor_init(void)
{

	//move to setting.

}    /*    MIPI_sensor_Init  */


static void preview_setting(void)
{

	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x03, 0x01);
	write_cmos_sensor(0x04, 0x73);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x01, 0x01);
	write_cmos_sensor(0x11, 0x30);
	write_cmos_sensor(0x33, 0x50);
	write_cmos_sensor(0x1c, 0x0c);
	write_cmos_sensor(0x1e, 0x80);
	write_cmos_sensor(0x29, 0x80);
	write_cmos_sensor(0x2a, 0xda);
	write_cmos_sensor(0x2c, 0x60);
	write_cmos_sensor(0x21, 0x26);
	write_cmos_sensor(0x25, 0x13);
	write_cmos_sensor(0x27, 0x00);
	write_cmos_sensor(0x55, 0x10);
	write_cmos_sensor(0x66, 0x36);
	write_cmos_sensor(0x68, 0x28);
	write_cmos_sensor(0x72, 0x50);
	write_cmos_sensor(0x58, 0x2a);
	write_cmos_sensor(0x75, 0x60);
	write_cmos_sensor(0x76, 0x05);
	write_cmos_sensor(0x51, 0x30);
	write_cmos_sensor(0x52, 0x2a);
	write_cmos_sensor(0x9d, 0x96);
	write_cmos_sensor(0xd0, 0x03);
	write_cmos_sensor(0xd1, 0x01);
	write_cmos_sensor(0xd2, 0xd0);
	write_cmos_sensor(0xd3, 0x02);
	write_cmos_sensor(0xd4, 0x40);
	write_cmos_sensor(0xfb, 0x7b);
	write_cmos_sensor(0xf0, 0x00);
	write_cmos_sensor(0xf1, 0x00);
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xac, 0x01);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xfd, 0x01);

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	write_cmos_sensor(0x3f, 0x03);
#endif

	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x1d, 0x01);
	write_cmos_sensor(0x8a, 0x0f);


}    /*    preview_setting  */


static void capture_setting(kal_uint16 currefps)
{
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x03, 0x01);
	write_cmos_sensor(0x04, 0x73);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x01, 0x01);
	write_cmos_sensor(0x11, 0x30);
	write_cmos_sensor(0x33, 0x50);
	write_cmos_sensor(0x1c, 0x0c);
	write_cmos_sensor(0x1e, 0x80);
	write_cmos_sensor(0x29, 0x80);
	write_cmos_sensor(0x2a, 0xda);
	write_cmos_sensor(0x2c, 0x60);
	write_cmos_sensor(0x21, 0x26);
	write_cmos_sensor(0x25, 0x13);
	write_cmos_sensor(0x27, 0x00);
	write_cmos_sensor(0x55, 0x10);
	write_cmos_sensor(0x66, 0x36);
	write_cmos_sensor(0x68, 0x28);
	write_cmos_sensor(0x72, 0x50);
	write_cmos_sensor(0x58, 0x2a);
	write_cmos_sensor(0x75, 0x60);
	write_cmos_sensor(0x76, 0x05);
	write_cmos_sensor(0x51, 0x30);
	write_cmos_sensor(0x52, 0x2a);
	write_cmos_sensor(0x9d, 0x96);
	write_cmos_sensor(0xd0, 0x03);
	write_cmos_sensor(0xd1, 0x01);
	write_cmos_sensor(0xd2, 0xd0);
	write_cmos_sensor(0xd3, 0x02);
	write_cmos_sensor(0xd4, 0x40);
	write_cmos_sensor(0xfb, 0x7b);
	write_cmos_sensor(0xf0, 0x00);
	write_cmos_sensor(0xf1, 0x00);
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xac, 0x01);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xfd, 0x01);

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	write_cmos_sensor(0x3f, 0x03);
#endif

	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x1d, 0x01);
	write_cmos_sensor(0x8a, 0x0f);
}    /*    capture_setting  */

static void normal_video_setting(kal_uint16 currefps)
{
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x03, 0x01);
	write_cmos_sensor(0x04, 0x73);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x01, 0x01);
	write_cmos_sensor(0x11, 0x30);
	write_cmos_sensor(0x33, 0x50);
	write_cmos_sensor(0x1c, 0x0c);
	write_cmos_sensor(0x1e, 0x80);
	write_cmos_sensor(0x29, 0x80);
	write_cmos_sensor(0x2a, 0xda);
	write_cmos_sensor(0x2c, 0x60);
	write_cmos_sensor(0x21, 0x26);
	write_cmos_sensor(0x25, 0x13);
	write_cmos_sensor(0x27, 0x00);
	write_cmos_sensor(0x55, 0x10);
	write_cmos_sensor(0x66, 0x36);
	write_cmos_sensor(0x68, 0x28);
	write_cmos_sensor(0x72, 0x50);
	write_cmos_sensor(0x58, 0x2a);
	write_cmos_sensor(0x75, 0x60);
	write_cmos_sensor(0x76, 0x05);
	write_cmos_sensor(0x51, 0x30);
	write_cmos_sensor(0x52, 0x2a);
	write_cmos_sensor(0x9d, 0x96);
	write_cmos_sensor(0xd0, 0x03);
	write_cmos_sensor(0xd1, 0x01);
	write_cmos_sensor(0xd2, 0xd0);
	write_cmos_sensor(0xd3, 0x02);
	write_cmos_sensor(0xd4, 0x40);
	write_cmos_sensor(0xfb, 0x7b);
	write_cmos_sensor(0xf0, 0x00);
	write_cmos_sensor(0xf1, 0x00);
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xac, 0x01);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xfd, 0x01);

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	write_cmos_sensor(0x3f, 0x03);
#endif

	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x1d, 0x01);
	write_cmos_sensor(0x8a, 0x0f);
}    /*    preview_setting  */


static void video_1080p_setting(void)
{
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x03, 0x01);
	write_cmos_sensor(0x04, 0x73);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x01, 0x01);
	write_cmos_sensor(0x11, 0x30);
	write_cmos_sensor(0x33, 0x50);
	write_cmos_sensor(0x1c, 0x0c);
	write_cmos_sensor(0x1e, 0x80);
	write_cmos_sensor(0x29, 0x80);
	write_cmos_sensor(0x2a, 0xda);
	write_cmos_sensor(0x2c, 0x60);
	write_cmos_sensor(0x21, 0x26);
	write_cmos_sensor(0x25, 0x13);
	write_cmos_sensor(0x27, 0x00);
	write_cmos_sensor(0x55, 0x10);
	write_cmos_sensor(0x66, 0x36);
	write_cmos_sensor(0x68, 0x28);
	write_cmos_sensor(0x72, 0x50);
	write_cmos_sensor(0x58, 0x2a);
	write_cmos_sensor(0x75, 0x60);
	write_cmos_sensor(0x76, 0x05);
	write_cmos_sensor(0x51, 0x30);
	write_cmos_sensor(0x52, 0x2a);
	write_cmos_sensor(0x9d, 0x96);
	write_cmos_sensor(0xd0, 0x03);
	write_cmos_sensor(0xd1, 0x01);
	write_cmos_sensor(0xd2, 0xd0);
	write_cmos_sensor(0xd3, 0x02);
	write_cmos_sensor(0xd4, 0x40);
	write_cmos_sensor(0xfb, 0x7b);
	write_cmos_sensor(0xf0, 0x00);
	write_cmos_sensor(0xf1, 0x00);
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xac, 0x01);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xfd, 0x01);

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	write_cmos_sensor(0x3f, 0x03);
#endif

	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x1d, 0x01);
	write_cmos_sensor(0x8a, 0x0f);


}    /*    preview_setting  */

static void video_720p_setting(void)
{
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x03, 0x01);
	write_cmos_sensor(0x04, 0x73);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x01, 0x01);
	write_cmos_sensor(0x11, 0x30);
	write_cmos_sensor(0x33, 0x50);
	write_cmos_sensor(0x1c, 0x0c);
	write_cmos_sensor(0x1e, 0x80);
	write_cmos_sensor(0x29, 0x80);
	write_cmos_sensor(0x2a, 0xda);
	write_cmos_sensor(0x2c, 0x60);
	write_cmos_sensor(0x21, 0x26);
	write_cmos_sensor(0x25, 0x13);
	write_cmos_sensor(0x27, 0x00);
	write_cmos_sensor(0x55, 0x10);
	write_cmos_sensor(0x66, 0x36);
	write_cmos_sensor(0x68, 0x28);
	write_cmos_sensor(0x72, 0x50);
	write_cmos_sensor(0x58, 0x2a);
	write_cmos_sensor(0x75, 0x60);
	write_cmos_sensor(0x76, 0x05);
	write_cmos_sensor(0x51, 0x30);
	write_cmos_sensor(0x52, 0x2a);
	write_cmos_sensor(0x9d, 0x96);
	write_cmos_sensor(0xd0, 0x03);
	write_cmos_sensor(0xd1, 0x01);
	write_cmos_sensor(0xd2, 0xd0);
	write_cmos_sensor(0xd3, 0x02);
	write_cmos_sensor(0xd4, 0x40);
	write_cmos_sensor(0xfb, 0x7b);
	write_cmos_sensor(0xf0, 0x00);
	write_cmos_sensor(0xf1, 0x00);
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xac, 0x01);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xfd, 0x01);

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	write_cmos_sensor(0x3f, 0x03);
#endif
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x1d, 0x01);
	write_cmos_sensor(0x8a, 0x0f);
}    /*    preview_setting  */


static void hs_video_setting(void)
{
	LOG_INF("E\n");

	video_1080p_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");

	video_720p_setting();
}

static void custom1_setting(void)
{
	LOG_INF("custom1 E\n");
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x03, 0x01);
	write_cmos_sensor(0x04, 0x73);
#if STEREO_CUSTOM1_30FPS
	LOG_INF("custom1_setting 30 fps\n");
#else
	LOG_INF("custom1_setting 24 fps\n");
	write_cmos_sensor(0x05, 0x01);
	write_cmos_sensor(0x06, 0x3e);
#endif
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x01, 0x01);
	write_cmos_sensor(0x11, 0x30);
	write_cmos_sensor(0x33, 0x50);
	write_cmos_sensor(0x1c, 0x0c);
	write_cmos_sensor(0x1e, 0x80);
	write_cmos_sensor(0x29, 0x80);
	write_cmos_sensor(0x2a, 0xda);
	write_cmos_sensor(0x2c, 0x60);
	write_cmos_sensor(0x21, 0x26);
	write_cmos_sensor(0x25, 0x13);
	write_cmos_sensor(0x27, 0x00);
	write_cmos_sensor(0x55, 0x10);
	write_cmos_sensor(0x66, 0x36);
	write_cmos_sensor(0x68, 0x28);
	write_cmos_sensor(0x72, 0x50);
	write_cmos_sensor(0x58, 0x2a);
	write_cmos_sensor(0x75, 0x60);
	write_cmos_sensor(0x76, 0x05);
	write_cmos_sensor(0x51, 0x30);
	write_cmos_sensor(0x52, 0x2a);
	write_cmos_sensor(0x9d, 0x96);
	write_cmos_sensor(0xd0, 0x03);
	write_cmos_sensor(0xd1, 0x01);
	write_cmos_sensor(0xd2, 0xd0);
	write_cmos_sensor(0xd3, 0x02);
	write_cmos_sensor(0xd4, 0x40);
	write_cmos_sensor(0xfb, 0x7b);
	write_cmos_sensor(0xf0, 0x00);
	write_cmos_sensor(0xf1, 0x00);
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xac, 0x01);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xfd, 0x01);

#if defined(SP250A_17201532) && defined(SP250A_MIPI_RAW_AS_MAIN2)
	write_cmos_sensor(0x3f, 0x03);
#endif

	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x1d, 0x01);
	write_cmos_sensor(0x8a, 0x0f);
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
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("gpw i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("gpw Read sensor id fail, write id:0x%x id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
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

	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("gpw i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("gpw Read sensor id fail, write id:0x%x id: 0x%x\n",
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

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	kdSetI2CSpeed(400);
#endif

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
}    /*    open  */



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
	LOG_INF("E\n");

	/*No Need to implement this function*/

	return ERROR_NONE;
}    /*    close  */


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
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

	return ERROR_NONE;
}    /*    preview   */

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
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps ==
	    imgsensor_info.cap1.max_framerate) {//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.cap1.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;
}    /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			       *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);

	return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			   *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

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

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			     *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();

	return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	//mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
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


	sensor_resolution->SensorHighSpeedVideoWidth     =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight     =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth     =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight     =
		imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width  =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height     =
		imgsensor_info.custom1.grabwindow_height;

	return ERROR_NONE;
}    /*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM
			   scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	//sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
	//sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
	//imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType =
		imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
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
	sensor_info->Custom1DelayFrame =
		imgsensor_info.custom1_delay_frame; //modify dualcam IDLHLYE-99 by wei.miao@reallytek.com 180629

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame; /*  The delay frame of setting frame length   */     //modify dualcam IDLHLYE-99 by wei.miao@reallytek.com 180629
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
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
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
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
		custom1(image_window, sensor_config_data); // custom1
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}    /* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150)
		 && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable,
					UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id,
		framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			       imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
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
		frame_length = imgsensor_info.normal_video.pclk / framerate *
			       10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.normal_video.framelength) ? (frame_length -
							imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength
					 + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frame_length = imgsensor_info.cap.pclk / framerate * 10 /
			       imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.cap.framelength) ? (frame_length -
							imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 /
			       imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.hs_video.framelength) ? (frame_length -
							imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 /
			       imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.slim_video.framelength) ? (frame_length -
							imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 /
			       imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.custom1.framelength) ? (frame_length -
							imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:  //coding with  preview scenario by default
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			       imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.pre.framelength) ? (frame_length -
							imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario \n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	default:
		break;
	}
	LOG_INF("scenario_id = %d *framerate = %d\n", scenario_id,
		*framerate);
	return ERROR_NONE;
}



static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x0d, 0x01);
	} else {
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x0d, 0x00);
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	/*
		if (enable)
			write_cmos_sensor(0x3f, 0x91);
		else
			write_cmos_sensor(0x3f, 0x00);
			mdelay(10);
	*/
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM
				  feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)
					   feature_para;
	//unsigned long long *feature_return_para=(unsigned long long *) feature_para;

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
		LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor0(sensor_reg_data->RegAddr,
				   sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor0(
						   sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
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
				      *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
					      *feature_data, *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
						  *(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
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
		imgsensor.ihdr_en = (BOOL) * feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*
				(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		break;

	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.cap.pclk /
				 (imgsensor_info.cap.linelength - 80)) *
				imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.normal_video.pclk /
				 (imgsensor_info.normal_video.linelength - 80)) *
				imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.hs_video.pclk /
				 (imgsensor_info.hs_video.linelength - 80)) *
				imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.slim_video.pclk /
				 (imgsensor_info.slim_video.linelength - 80)) *
				imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.custom1.pclk /
				 (imgsensor_info.custom1.linelength - 80)) *
				imgsensor_info.custom1.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.pre.pclk /
				 (imgsensor_info.pre.linelength - 80)) *
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		LOG_INF("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME shutter :%d framelen :%d\n",
			(UINT16)*feature_data, (UINT16) *(feature_data + 1));
		set_shutter_frame_length((UINT16)*feature_data,
					 (UINT16) *(feature_data + 1));
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

UINT32 SP250A_MIPI_RAW_SensorInit(
	struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}

