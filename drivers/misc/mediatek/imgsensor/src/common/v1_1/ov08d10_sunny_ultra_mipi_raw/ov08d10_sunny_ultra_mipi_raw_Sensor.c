/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

#include "ov08d10_sunny_ultra_mipi_raw_Sensor.h"

#define PFX "OV08D10SUNNYULTRA_camera_sensor"
#define LOG_INF(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)
 /* N17 code for HQ-293327 by changqi at 2023/05/16 start */
#define VENDOR_ID 0x01
 /* N17 code for HQ-293327 by changqi at 2023/05/16 start */
/* Camera Hardwareinfo */
//extern struct global_otp_struct hw_info_main2_otp;

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV08D10_SUNNY_ULTRA_SENSOR_ID,

	.checksum_value = 0xdac1f07c,       //0x6d01485c // Auto Test Mode
/* N17 code for HQ-293327 by changqi start*/
	.pre = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 460,            //record different mode's linelength
		.framelength = 2608,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 3264,        //record different mode's width of grabwindow
		.grabwindow_height = 2448,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
	},
	.cap = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 460,            //record different mode's linelength
		.framelength = 2608,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 3264,        //record different mode's width of grabwindow
		.grabwindow_height = 2448,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
	},

	.normal_video = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 460,            //record different mode's linelength
		.framelength = 2608,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 3264,        //record different mode's width of grabwindow
		.grabwindow_height = 1836,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
	},
	.hs_video = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 460,            //record different mode's linelength
		.framelength = 2608,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 3264,        //record different mode's width of grabwindow
		.grabwindow_height = 2448,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
	},
	.slim_video = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 478,            //record different mode's linelength
		.framelength = 2504,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1632,        //record different mode's width of grabwindow
		.grabwindow_height = 1224,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 144000000,
	},
    .custom1 = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 478,            //record different mode's linelength
		.framelength = 3136,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1632,        //record different mode's width of grabwindow
		.grabwindow_height = 1224,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 240,
		.mipi_pixel_rate = 144000000,
    },
    .custom2 = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 478,            //record different mode's linelength
		.framelength = 2504,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1632,        //record different mode's width of grabwindow
		.grabwindow_height = 1224,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 144000000,
    },
    .custom3 = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 460,            //record different mode's linelength
		.framelength = 2608,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 3264,        //record different mode's width of grabwindow
		.grabwindow_height = 2448,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
    },
    .custom4 = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 460,            //record different mode's linelength
		.framelength = 2608,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 3264,        //record different mode's width of grabwindow
		.grabwindow_height = 2448,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
    },
    .custom5 = {
		.pclk = 36000000,            //record different mode's pclk
		.linelength = 460,            //record different mode's linelength
		.framelength = 2608,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 3264,        //record different mode's width of grabwindow
		.grabwindow_height = 2448,        //record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
    },
	.margin = 20,            //sensor framelength & shutter margin
	.min_shutter = 4,        //min shutter
	.min_gain = BASEGAIN, // 1x gain
	.max_gain = 15.5 * BASEGAIN, // real again is 15.5x
	.min_gain_iso = 50,
/* N17 code for HQ-293327 by changqi at 2023/05/16 start */
	.gain_step = 4,    //min sensor gain step 0.0625 * BASEGAIN
/* N17 code for HQ-293327 by changqi at 2023/05/16 end */
	.gain_type = 1,
	.max_frame_length = 0x7fEE,//max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle

	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 6,      //support sensor mode num

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
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 start */
    .frame_time_delay_frame = 2,
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 end */
	.isp_driving_current = ISP_DRIVING_6MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,//sensor output first pixel color
	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_2_LANE,//mipi lane num
	.i2c_addr_table = {0x6c,0x20,0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,                //mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,                    //current shutter
	.gain = 0x100,                        //current gain
	.dummy_pixel = 0,                    //current dummypixel
	.dummy_line = 0,                    //current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,        //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x6c,//record current sensor's i2c write id
};



/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10]={
	{  3264, 2448,  0, 0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448,  0, 0, 3264, 2448}, // Preview
	{  3264, 2448,  0, 0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448,  0, 0, 3264, 2448}, // capture
	{  3264, 2448,  0, 0, 3264, 2448, 3264, 2448, 0000, 0306, 3264, 1836,  0, 0, 3264, 1836}, // video
	{  3264, 2448,  0, 0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448,  0, 0, 3264, 2448}, //hight speed video
	{  3264, 2448,  0, 0, 3264, 2448, 1632, 1224, 0000, 0000, 1632, 1224,  0, 0, 1632, 1224}, // slim video
	{  3264, 2448,  0, 0, 3264, 2448, 1632, 1224, 0000, 0000, 1632, 1224,  0, 0, 1632, 1224},  // custom1
	{  3264, 2448,  0, 0, 3264, 2448, 1632, 1224, 0000, 0000, 1632, 1224,  0, 0, 1632, 1224}, // custom2
	{  3264, 2448,  0, 0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448,  0, 0, 3264, 2448}, // custom3
	{  3264, 2448,  0, 0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448,  0, 0, 3264, 2448}, // custom4
	{  3264, 2448,  0, 0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448,  0, 0, 3264, 2448}, // custom5
};
/* N17 code for HQ-293327 by changqi end*/


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	//kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	char pu_send_cmd[1] = {(char)(addr & 0xFF)};
	iReadRegI2C(pu_send_cmd, 1, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = {(char)(addr & 0xFF), (char)(para & 0xFF)};
	//kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}

#if 0
#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3
#endif

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
      puSendCmd[tosend++] = (char)(data & 0xFF);
      IDX += 2;
      addr_last = addr;
    }
#if MULTI_WRITE
    /* Write when remain buffer size is less than 3 bytes or reach end of data */
    if ((I2C_BUFFER_LEN - tosend) < 3 || IDX == len || addr != addr_last) {
      iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id, 3,
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
#endif

static void set_dummy(void)
{

	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */
	write_cmos_sensor(0xfd, 0x01);
/* N17 code for HQ-293330 by xuyanfei at 2023/05/20 start */
    write_cmos_sensor(0x05, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2) & 0xFF00) >> 8);
/* N17 code for HQ-293330 by xuyanfei at 2023/05/20 end */
    write_cmos_sensor(0x06, ((imgsensor.frame_length - imgsensor.vblank_convert )* 2) & 0xFF);
	write_cmos_sensor(0xfd, 0x01);	//page1
	write_cmos_sensor(0x01, 0x01);	//fresh

}    /*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	write_cmos_sensor(0xfd, 0x00);
	return (((read_cmos_sensor(0x00) << 24) |(read_cmos_sensor(0x01) << 16) |(read_cmos_sensor(0x02) << 8) | read_cmos_sensor(0x03))+1);
}
static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
	//imgsensor.dummy_line = 0;
	//else
	//imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	set_dummy();
}    /*    set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint32 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?	imgsensor_info.min_shutter : shutter;

	shutter =
		(shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	//frame_length and shutter should be an even number.
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
//auroflicker:need to avoid 15fps and 30 fps
	if (imgsensor.autoflicker_en == KAL_TRUE) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			realtime_fps = 296;
	        set_max_framerate(realtime_fps, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			realtime_fps = 146;
	        set_max_framerate(realtime_fps, 0);
		}
    }
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x02, (shutter*2 >> 16) & 0xFF);
		write_cmos_sensor(0x03, (shutter*2 >> 8) & 0xFF);
		write_cmos_sensor(0x04,  shutter*2  & 0xFF);
		write_cmos_sensor(0xfd, 0x01);	//page1
		write_cmos_sensor(0x01, 0x01);	//fresh

   	 //LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
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
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);

}
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 start */
static void set_shutter_frame_length(kal_uint16 shutter,
			kal_uint16 frame_length,kal_bool auto_extend_en)
{
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
   /* if shutter bigger than frame_length, should extend frame length first */
    if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;

	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

    if (shutter > imgsensor.frame_length - imgsensor_info.margin)
           imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter =
		(shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	//frame_length and shutter should be an even number.
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
//auroflicker:need to avoid 15fps and 30 fps
	if (imgsensor.autoflicker_en ) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			realtime_fps = 296;
	    set_max_framerate(realtime_fps, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			realtime_fps = 146;
	    set_max_framerate(realtime_fps, 0);
		}
/* N17 code for HQ-293330 by xuyanfei at 2023/05/20 start */
		else{
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x05, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2) & 0xFF00) >> 8);
		write_cmos_sensor(0x06, ((imgsensor.frame_length - imgsensor.vblank_convert )* 2) & 0xFF);
		write_cmos_sensor(0xfd, 0x01);	//page1
		write_cmos_sensor(0x01, 0x01);	//fresh
		}
	}
	else{
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x05, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2) & 0xFF00) >> 8);
		write_cmos_sensor(0x06, ((imgsensor.frame_length - imgsensor.vblank_convert )* 2) & 0xFF);
		write_cmos_sensor(0xfd, 0x01);	//page1
		write_cmos_sensor(0x01, 0x01);	//fresh
/* N17 code for HQ-293330 by xuyanfei at 2023/05/20 end */
	}
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x02, (shutter*2 >> 16) & 0xFF);
		write_cmos_sensor(0x03, (shutter*2 >> 8) & 0xFF);
		write_cmos_sensor(0x04, shutter*2  & 0xFF);
		write_cmos_sensor(0xfd, 0x01);	//page1
		write_cmos_sensor(0x01, 0x01);	//fresh


/* N17 code for HQ-293330 by xuyanfei at 2023/05/20 start */
	LOG_INF("Exit! shutter =%d, framelength =%d/%d, dummy_line=%d\n", shutter, imgsensor.frame_length, frame_length, dummy_line);
/* N17 code for HQ-293330 by xuyanfei at 2023/05/20 end */
}				/* set_shutter_frame_length */
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 end */


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
	
	if ((gain < BASEGAIN) || (gain > 15.5*BASEGAIN)){
		LOG_INF("error gain setting");
		
			if (gain <BASEGAIN) gain=BASEGAIN;
			if (gain >15.5*BASEGAIN) gain=15.5*BASEGAIN;
	}
		
		iReg = 0x10 * gain/BASEGAIN;        //change mtk gain base to aptina gain base

		if(iReg<=0x10)
		{
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, 0x10);//0x23
			write_cmos_sensor(0x01, 0x01);	//fresh
			LOG_INF("OV02B1BMIPI_SetGain = 16");
		}
		else if(iReg>= 0xf8)//gpw
		{
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, 0xf8);
			write_cmos_sensor(0x01, 0x01);	//fresh
			LOG_INF("OV02B1BMIPI_SetGain = 160");
		}
		else
		{
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x24, (kal_uint8)iReg);
			write_cmos_sensor(0x01, 0x01);	//fresh
			LOG_INF("OV02B1BMIPI_SetGain = %d",iReg);
		}


	return gain;
}    /*    set_gain  */

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	/********************************************************
	 *

	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/

	switch (image_mirror) {
	case IMAGE_NORMAL:

		break;
	case IMAGE_H_MIRROR:

		break;
	case IMAGE_V_MIRROR:

		break;
	case IMAGE_HV_MIRROR:

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

}
/* N17 code for HQ-293327 by changqi start*/
static void preview_setting(void)
{
	//3264X2448_30fps
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
    mdelay(3);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x02, 0x00);
	write_cmos_sensor(0x03, 0x12);
	write_cmos_sensor(0x04, 0x50);
	write_cmos_sensor(0x05, 0x00);
	write_cmos_sensor(0x06, 0xd0);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0x30);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x11);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x40);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x0D, 0x01);
	write_cmos_sensor(0x0F, 0x01);
	write_cmos_sensor(0x10, 0x00);
	write_cmos_sensor(0x11, 0x00);
	write_cmos_sensor(0x12, 0x0C);
	write_cmos_sensor(0x13, 0xCF);
	write_cmos_sensor(0x14, 0x00);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x18, 0x00);
	write_cmos_sensor(0x19, 0x00);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	write_cmos_sensor(0xfd, 0x01);

}


static void capture_setting(kal_uint16 currefps)
{
	//3264X2448_30fps
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
    mdelay(3);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x02, 0x00);
	write_cmos_sensor(0x03, 0x12);
	write_cmos_sensor(0x04, 0x50);
	write_cmos_sensor(0x05, 0x00);
	write_cmos_sensor(0x06, 0xd0);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0x30);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x11);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x40);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x0D, 0x01);
	write_cmos_sensor(0x0F, 0x01);
	write_cmos_sensor(0x10, 0x00);
	write_cmos_sensor(0x11, 0x00);
	write_cmos_sensor(0x12, 0x0C);
	write_cmos_sensor(0x13, 0xCF);
	write_cmos_sensor(0x14, 0x00);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x18, 0x00);
	write_cmos_sensor(0x19, 0x00);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	write_cmos_sensor(0xfd, 0x01);


}    /*    capture_setting  */

static void normal_video_setting(void)
{
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x20,0x0e);
	mdelay(3);
	write_cmos_sensor(0x20,0x0b);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x11,0x2a);
	write_cmos_sensor(0x14,0x43);
	write_cmos_sensor(0x1e,0x23);
	write_cmos_sensor(0x16,0x82);
	write_cmos_sensor(0x21,0x00);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0x12,0x00);
	write_cmos_sensor(0x02,0x00);
	write_cmos_sensor(0x03,0x12);
	write_cmos_sensor(0x04,0x50);
	write_cmos_sensor(0x05,0x00);
	write_cmos_sensor(0x06,0xd0);
	write_cmos_sensor(0x07,0x05);
	write_cmos_sensor(0x21,0x02);
	write_cmos_sensor(0x24,0x30);
	write_cmos_sensor(0x33,0x03);
	write_cmos_sensor(0x01,0x03);
	write_cmos_sensor(0x19,0x10);
	write_cmos_sensor(0x42,0x55);
	write_cmos_sensor(0x43,0x00);
	write_cmos_sensor(0x47,0x07);
	write_cmos_sensor(0x48,0x08);
	write_cmos_sensor(0x4c,0x38);
	write_cmos_sensor(0xb2,0x7e);
	write_cmos_sensor(0xb3,0x7b);
	write_cmos_sensor(0xbd,0x08);
	write_cmos_sensor(0xd2,0x47);
	write_cmos_sensor(0xd3,0x10);
	write_cmos_sensor(0xd4,0x0d);
	write_cmos_sensor(0xd5,0x08);
	write_cmos_sensor(0xd6,0x07);
	write_cmos_sensor(0xb1,0x00);
	write_cmos_sensor(0xb4,0x00);
	write_cmos_sensor(0xb7,0x0a);
	write_cmos_sensor(0xbc,0x44);
	write_cmos_sensor(0xbf,0x42);
	write_cmos_sensor(0xc1,0x10);
	write_cmos_sensor(0xc3,0x24);
	write_cmos_sensor(0xc8,0x03);
	write_cmos_sensor(0xc9,0xf8);
	write_cmos_sensor(0xe1,0x33);
	write_cmos_sensor(0xe2,0xbb);
	write_cmos_sensor(0x51,0x0c);
	write_cmos_sensor(0x52,0x0a);
	write_cmos_sensor(0x57,0x8c);
	write_cmos_sensor(0x59,0x09);
	write_cmos_sensor(0x5a,0x08);
	write_cmos_sensor(0x5e,0x10);
	write_cmos_sensor(0x60,0x02);
	write_cmos_sensor(0x6d,0x5c);
	write_cmos_sensor(0x76,0x16);
	write_cmos_sensor(0x7c,0x11);
	write_cmos_sensor(0x90,0x28);
	write_cmos_sensor(0x91,0x16);
	write_cmos_sensor(0x92,0x1c);
	write_cmos_sensor(0x93,0x24);
	write_cmos_sensor(0x95,0x48);
	write_cmos_sensor(0x9c,0x06);
	write_cmos_sensor(0xca,0x0c);
	write_cmos_sensor(0xce,0x0d);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xdd,0x18);
	write_cmos_sensor(0xde,0x19);
	write_cmos_sensor(0xdf,0x32);
	write_cmos_sensor(0xe0,0x70);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0xc2,0x05);
	write_cmos_sensor(0xd7,0x88);
	write_cmos_sensor(0xd8,0x77);
	write_cmos_sensor(0xd9,0x66);
	write_cmos_sensor(0xfd,0x07);
	write_cmos_sensor(0x00,0xf8);
	write_cmos_sensor(0x01,0x2b);
	write_cmos_sensor(0x05,0x40);
	write_cmos_sensor(0x08,0x06);
	write_cmos_sensor(0x09,0x11);
	write_cmos_sensor(0x28,0x6f);
	write_cmos_sensor(0x2a,0x20);
	write_cmos_sensor(0x2b,0x05);
	write_cmos_sensor(0x5e,0x10);
	write_cmos_sensor(0x52,0x00);
	write_cmos_sensor(0x53,0x80);
	write_cmos_sensor(0x54,0x00);
	write_cmos_sensor(0x55,0x80);
	write_cmos_sensor(0x56,0x00);
	write_cmos_sensor(0x57,0x80);
	write_cmos_sensor(0x58,0x00);
	write_cmos_sensor(0x59,0x80);
	write_cmos_sensor(0x5c,0x3f);
	write_cmos_sensor(0xfd,0x02);
	write_cmos_sensor(0x9a,0x30);
	write_cmos_sensor(0xa8,0x02);
	write_cmos_sensor(0xfd,0x02);
	write_cmos_sensor(0xa0,0x01);
	write_cmos_sensor(0xa1,0x3a);
	write_cmos_sensor(0xa2,0x07);
	write_cmos_sensor(0xa3,0x2c);
	write_cmos_sensor(0xa4,0x00);
	write_cmos_sensor(0xa5,0x08);
	write_cmos_sensor(0xa6,0x0c);
	write_cmos_sensor(0xa7,0xc0);
	write_cmos_sensor(0xfd,0x05);
	write_cmos_sensor(0x04,0x40);
	write_cmos_sensor(0x07,0x00);
	write_cmos_sensor(0x0D,0x01);
	write_cmos_sensor(0x0F,0x01);
	write_cmos_sensor(0x10,0x00);
	write_cmos_sensor(0x11,0x00);
	write_cmos_sensor(0x12,0x0C);
	write_cmos_sensor(0x13,0xCF);
	write_cmos_sensor(0x14,0x00);
	write_cmos_sensor(0x15,0x00);
	write_cmos_sensor(0x18,0x00);
	write_cmos_sensor(0x19,0x00);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x24,0x01);
	write_cmos_sensor(0xc0,0x16);
	write_cmos_sensor(0xc1,0x08);
	write_cmos_sensor(0xc2,0x30);
	write_cmos_sensor(0x8e,0x0c);
	write_cmos_sensor(0x8f,0xc0);
	write_cmos_sensor(0x90,0x07);
	write_cmos_sensor(0x91,0x2c);
	write_cmos_sensor(0xb7,0x02);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x20,0x0f);
	write_cmos_sensor(0xe7,0x03);
	write_cmos_sensor(0xe7,0x00);
	write_cmos_sensor(0xfd,0x01);
}/*   video_setting  */

static void hs_video_setting(void)
{
	//3264X2448_30fps
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
    mdelay(3);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x02, 0x00);
	write_cmos_sensor(0x03, 0x12);
	write_cmos_sensor(0x04, 0x50);
	write_cmos_sensor(0x05, 0x00);
	write_cmos_sensor(0x06, 0xd0);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0x30);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x11);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x40);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x0D, 0x01);
	write_cmos_sensor(0x0F, 0x01);
	write_cmos_sensor(0x10, 0x00);
	write_cmos_sensor(0x11, 0x00);
	write_cmos_sensor(0x12, 0x0C);
	write_cmos_sensor(0x13, 0xCF);
	write_cmos_sensor(0x14, 0x00);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x18, 0x00);
	write_cmos_sensor(0x19, 0x00);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	write_cmos_sensor(0xfd, 0x01);
}

static void slim_video_setting(void)
{
	//1632X1224_30fps
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	mdelay(2);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x1d, 0x00);
	write_cmos_sensor(0x18, 0x3c);
	write_cmos_sensor(0x1c, 0x19);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x1a, 0x0a);
	write_cmos_sensor(0x1b, 0x08);
	write_cmos_sensor(0x2a, 0x01);
	write_cmos_sensor(0x2b, 0x9a);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x02, 0x00);
	write_cmos_sensor(0x03, 0x09);
	write_cmos_sensor(0x04, 0x6a);
	write_cmos_sensor(0x05, 0x09);
	write_cmos_sensor(0x06, 0xc8);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0xf8);
	write_cmos_sensor(0x31, 0x06);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x1a);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x03);
	write_cmos_sensor(0x09, 0x08);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x2c, 0x01);
	write_cmos_sensor(0x50, 0x02);
	write_cmos_sensor(0x51, 0x03);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa9, 0x04);
	write_cmos_sensor(0xaa, 0xd0);
	write_cmos_sensor(0xab, 0x06);
	write_cmos_sensor(0xac, 0x68);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xa2, 0x04);
	write_cmos_sensor(0xa3, 0xc8);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x04);
	write_cmos_sensor(0xa6, 0x06);
	write_cmos_sensor(0xa7, 0x60);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x60);
	write_cmos_sensor(0x06, 0x80);
	write_cmos_sensor(0x07, 0x99);
	write_cmos_sensor(0x0D, 0x03);
	write_cmos_sensor(0x0F, 0x03);
	write_cmos_sensor(0x10, 0x00);
	write_cmos_sensor(0x11, 0x00);
	write_cmos_sensor(0x12, 0x0C);
	write_cmos_sensor(0x13, 0xCF);
	write_cmos_sensor(0x14, 0x00);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x18, 0x06);
	write_cmos_sensor(0x19, 0x68);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x06);
	write_cmos_sensor(0x8f, 0x60);
	write_cmos_sensor(0x90, 0x04);
	write_cmos_sensor(0x91, 0xc8);
	write_cmos_sensor(0x93, 0x0e);
	write_cmos_sensor(0x94, 0x77);
	write_cmos_sensor(0x95, 0x77);
	write_cmos_sensor(0x96, 0x10);
	write_cmos_sensor(0x98, 0x88);
	write_cmos_sensor(0x9c, 0x1a);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	write_cmos_sensor(0xfd, 0x01);
}
static void custom1_setting(void)
{
	//1632x1224_24fps
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x20,0x0e);
	mdelay(2);
	write_cmos_sensor(0x20,0x0b);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x1d,0x00);
	write_cmos_sensor(0x18,0x3c);
	write_cmos_sensor(0x1c,0x19);
	write_cmos_sensor(0x11,0x2a);
	write_cmos_sensor(0x14,0x43);
	write_cmos_sensor(0x1e,0x23);
	write_cmos_sensor(0x16,0x82);
	write_cmos_sensor(0x21,0x00);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0x1a,0x0a);
	write_cmos_sensor(0x1b,0x08);
	write_cmos_sensor(0x2a,0x01);
	write_cmos_sensor(0x2b,0x9a);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0x12,0x00);
	write_cmos_sensor(0x02,0x00);
	write_cmos_sensor(0x03,0x09);
	write_cmos_sensor(0x04,0x6a);
	write_cmos_sensor(0x05,0x0e);
	write_cmos_sensor(0x06,0xb8);
	write_cmos_sensor(0x07,0x05);
	write_cmos_sensor(0x21,0x02);
	write_cmos_sensor(0x24,0xf8);
	write_cmos_sensor(0x31,0x06);
	write_cmos_sensor(0x33,0x03);
	write_cmos_sensor(0x01,0x03);
	write_cmos_sensor(0x19,0x10);
	write_cmos_sensor(0x42,0x55);
	write_cmos_sensor(0x43,0x00);
	write_cmos_sensor(0x47,0x07);
	write_cmos_sensor(0x48,0x08);
	write_cmos_sensor(0x4c,0x38);
	write_cmos_sensor(0xb2,0x7e);
	write_cmos_sensor(0xb3,0x7b);
	write_cmos_sensor(0xbd,0x08);
	write_cmos_sensor(0xd2,0x47);
	write_cmos_sensor(0xd3,0x10);
	write_cmos_sensor(0xd4,0x0d);
	write_cmos_sensor(0xd5,0x08);
	write_cmos_sensor(0xd6,0x07);
	write_cmos_sensor(0xb1,0x00);
	write_cmos_sensor(0xb4,0x00);
	write_cmos_sensor(0xb7,0x0a);
	write_cmos_sensor(0xbc,0x44);
	write_cmos_sensor(0xbf,0x42);
	write_cmos_sensor(0xc1,0x10);
	write_cmos_sensor(0xc3,0x24);
	write_cmos_sensor(0xc8,0x03);
	write_cmos_sensor(0xc9,0xf8);
	write_cmos_sensor(0xe1,0x33);
	write_cmos_sensor(0xe2,0xbb);
	write_cmos_sensor(0x51,0x0c);
	write_cmos_sensor(0x52,0x0a);
	write_cmos_sensor(0x57,0x8c);
	write_cmos_sensor(0x59,0x09);
	write_cmos_sensor(0x5a,0x08);
	write_cmos_sensor(0x5e,0x10);
	write_cmos_sensor(0x60,0x02);
	write_cmos_sensor(0x6d,0x5c);
	write_cmos_sensor(0x76,0x16);
	write_cmos_sensor(0x7c,0x1a);
	write_cmos_sensor(0x90,0x28);
	write_cmos_sensor(0x91,0x16);
	write_cmos_sensor(0x92,0x1c);
	write_cmos_sensor(0x93,0x24);
	write_cmos_sensor(0x95,0x48);
	write_cmos_sensor(0x9c,0x06);
	write_cmos_sensor(0xca,0x0c);
	write_cmos_sensor(0xce,0x0d);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xdd,0x18);
	write_cmos_sensor(0xde,0x19);
	write_cmos_sensor(0xdf,0x32);
	write_cmos_sensor(0xe0,0x70);
	write_cmos_sensor(0xfd,0x01);
	write_cmos_sensor(0xc2,0x05);
	write_cmos_sensor(0xd7,0x88);
	write_cmos_sensor(0xd8,0x77);
	write_cmos_sensor(0xd9,0x66);
	write_cmos_sensor(0xfd,0x07);
	write_cmos_sensor(0x00,0xf8);
	write_cmos_sensor(0x01,0x2b);
	write_cmos_sensor(0x05,0x40);
	write_cmos_sensor(0x08,0x03);
	write_cmos_sensor(0x09,0x08);
	write_cmos_sensor(0x28,0x6f);
	write_cmos_sensor(0x2a,0x20);
	write_cmos_sensor(0x2b,0x05);
	write_cmos_sensor(0x2c,0x01);
	write_cmos_sensor(0x50,0x02);
	write_cmos_sensor(0x51,0x03);
	write_cmos_sensor(0x5e,0x10);
	write_cmos_sensor(0x52,0x00);
	write_cmos_sensor(0x53,0x80);
	write_cmos_sensor(0x54,0x00);
	write_cmos_sensor(0x55,0x80);
	write_cmos_sensor(0x56,0x00);
	write_cmos_sensor(0x57,0x80);
	write_cmos_sensor(0x58,0x00);
	write_cmos_sensor(0x59,0x80);
	write_cmos_sensor(0x5c,0x3f);
	write_cmos_sensor(0xfd,0x02);
	write_cmos_sensor(0x9a,0x30);
	write_cmos_sensor(0xa8,0x02);
	write_cmos_sensor(0xfd,0x02);
	write_cmos_sensor(0xa9,0x04);
	write_cmos_sensor(0xaa,0xd0);
	write_cmos_sensor(0xab,0x06);
	write_cmos_sensor(0xac,0x68);
	write_cmos_sensor(0xfd,0x02);
	write_cmos_sensor(0xa0,0x00);
	write_cmos_sensor(0xa1,0x04);
	write_cmos_sensor(0xa2,0x04);
	write_cmos_sensor(0xa3,0xc8);
	write_cmos_sensor(0xa4,0x00);
	write_cmos_sensor(0xa5,0x04);
	write_cmos_sensor(0xa6,0x06);
	write_cmos_sensor(0xa7,0x60);
	write_cmos_sensor(0xfd,0x05);
	write_cmos_sensor(0xfd,0x05);
	write_cmos_sensor(0x04,0x60);
	write_cmos_sensor(0x06,0x80);
	write_cmos_sensor(0x07,0x99);
	write_cmos_sensor(0x0D,0x03);
	write_cmos_sensor(0x0F,0x03);
	write_cmos_sensor(0x10,0x00);
	write_cmos_sensor(0x11,0x00);
	write_cmos_sensor(0x12,0x0C);
	write_cmos_sensor(0x13,0xCF);
	write_cmos_sensor(0x14,0x00);
	write_cmos_sensor(0x15,0x00);
	write_cmos_sensor(0x18,0x06);
	write_cmos_sensor(0x19,0x68);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x24,0x01);
	write_cmos_sensor(0xc0,0x16);
	write_cmos_sensor(0xc1,0x08);
	write_cmos_sensor(0xc2,0x30);
	write_cmos_sensor(0x8e,0x06);
	write_cmos_sensor(0x8f,0x60);
	write_cmos_sensor(0x90,0x04);
	write_cmos_sensor(0x91,0xc8);
	write_cmos_sensor(0x93,0x0e);
	write_cmos_sensor(0x94,0x77);
	write_cmos_sensor(0x95,0x77);
	write_cmos_sensor(0x96,0x10);
	write_cmos_sensor(0x98,0x88);
	write_cmos_sensor(0x9c,0x1a);
	write_cmos_sensor(0xb7,0x02);
	write_cmos_sensor(0xfd,0x00);
	write_cmos_sensor(0x20,0x0f);
	write_cmos_sensor(0xe7,0x03);
	write_cmos_sensor(0xe7,0x00);
	write_cmos_sensor(0xfd,0x01);
/* N17 code for HQ-308970 by changqi at 2023/07/26 start*/
	// write_cmos_sensor(0xfd,0x00);
	// write_cmos_sensor(0xa0,0x01);
/* N17 code for HQ-308970 by changqi at 2023/07/26 end*/
}/*   stereo_setting  */
/* N17 code for HQ-293327 by changqi end*/
static void custom2_setting(void)
{
	//1632X1224_30fps
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	mdelay(2);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x1d, 0x00);
	write_cmos_sensor(0x18, 0x3c);
	write_cmos_sensor(0x1c, 0x19);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x1a, 0x0a);
	write_cmos_sensor(0x1b, 0x08);
	write_cmos_sensor(0x2a, 0x01);
	write_cmos_sensor(0x2b, 0x9a);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x02, 0x00);
	write_cmos_sensor(0x03, 0x09);
	write_cmos_sensor(0x04, 0x6a);
	write_cmos_sensor(0x05, 0x09);
	write_cmos_sensor(0x06, 0xc8);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0xf8);
	write_cmos_sensor(0x31, 0x06);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x1a);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x03);
	write_cmos_sensor(0x09, 0x08);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x2c, 0x01);
	write_cmos_sensor(0x50, 0x02);
	write_cmos_sensor(0x51, 0x03);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa9, 0x04);
	write_cmos_sensor(0xaa, 0xd0);
	write_cmos_sensor(0xab, 0x06);
	write_cmos_sensor(0xac, 0x68);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xa2, 0x04);
	write_cmos_sensor(0xa3, 0xc8);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x04);
	write_cmos_sensor(0xa6, 0x06);
	write_cmos_sensor(0xa7, 0x60);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x60);
	write_cmos_sensor(0x06, 0x80);
	write_cmos_sensor(0x07, 0x99);
	write_cmos_sensor(0x0D, 0x03);
	write_cmos_sensor(0x0F, 0x03);
	write_cmos_sensor(0x10, 0x00);
	write_cmos_sensor(0x11, 0x00);
	write_cmos_sensor(0x12, 0x0C);
	write_cmos_sensor(0x13, 0xCF);
	write_cmos_sensor(0x14, 0x00);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x18, 0x06);
	write_cmos_sensor(0x19, 0x68);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x06);
	write_cmos_sensor(0x8f, 0x60);
	write_cmos_sensor(0x90, 0x04);
	write_cmos_sensor(0x91, 0xc8);
	write_cmos_sensor(0x93, 0x0e);
	write_cmos_sensor(0x94, 0x77);
	write_cmos_sensor(0x95, 0x77);
	write_cmos_sensor(0x96, 0x10);
	write_cmos_sensor(0x98, 0x88);
	write_cmos_sensor(0x9c, 0x1a);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	write_cmos_sensor(0xfd, 0x01);
}

static void custom3_setting(void)
{
	//3264X2448_24fps
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
    mdelay(3);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x02, 0x00);
	write_cmos_sensor(0x03, 0x12);
	write_cmos_sensor(0x04, 0x50);
	write_cmos_sensor(0x05, 0x00);
	write_cmos_sensor(0x06, 0xd0);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0x30);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x11);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x40);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x0D, 0x01);
	write_cmos_sensor(0x0F, 0x01);
	write_cmos_sensor(0x10, 0x00);
	write_cmos_sensor(0x11, 0x00);
	write_cmos_sensor(0x12, 0x0C);
	write_cmos_sensor(0x13, 0xCF);
	write_cmos_sensor(0x14, 0x00);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x18, 0x00);
	write_cmos_sensor(0x19, 0x00);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	write_cmos_sensor(0xfd, 0x01);

}

static void custom4_setting(void)
{
	//3264X2448_20fps
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
    mdelay(3);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x02, 0x00);
	write_cmos_sensor(0x03, 0x12);
	write_cmos_sensor(0x04, 0x50);
	write_cmos_sensor(0x05, 0x00);
	write_cmos_sensor(0x06, 0xd0);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0x30);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x11);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x40);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x0D, 0x01);
	write_cmos_sensor(0x0F, 0x01);
	write_cmos_sensor(0x10, 0x00);
	write_cmos_sensor(0x11, 0x00);
	write_cmos_sensor(0x12, 0x0C);
	write_cmos_sensor(0x13, 0xCF);
	write_cmos_sensor(0x14, 0x00);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x18, 0x00);
	write_cmos_sensor(0x19, 0x00);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	write_cmos_sensor(0xfd, 0x01);

}

static void custom5_setting(void)
{
	//3264X2448_15fps
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
    mdelay(3);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x02, 0x00);
	write_cmos_sensor(0x03, 0x12);
	write_cmos_sensor(0x04, 0x50);
	write_cmos_sensor(0x05, 0x00);
	write_cmos_sensor(0x06, 0xd0);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0x30);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x11);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x40);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x0D, 0x01);
	write_cmos_sensor(0x0F, 0x01);
	write_cmos_sensor(0x10, 0x00);
	write_cmos_sensor(0x11, 0x00);
	write_cmos_sensor(0x12, 0x0C);
	write_cmos_sensor(0x13, 0xCF);
	write_cmos_sensor(0x14, 0x00);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x18, 0x00);
	write_cmos_sensor(0x19, 0x00);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	write_cmos_sensor(0xfd, 0x01);

}

static kal_uint16 get_vendor_id(void)
{
        kal_uint16 get_byte = 0;
        char pusendcmd[2] = {(char)(0x01 >> 8), (char)(0x01 & 0xFF) };

        iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA0);
        return get_byte;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
    kal_uint16 vendor_id = 0;
    vendor_id = get_vendor_id();
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
 /* N17 code for HQ-293327 by changqi at 2023/05/16 start */
			pr_info("ov08d10sunny sensor id : 0x%x, vendor id: 0x%x\n",*sensor_id, vendor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_info("ov08d10sunny i2c write id : 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
				if(vendor_id == VENDOR_ID){
					return ERROR_NONE;
				}
				else{
					pr_info("Read vendor id fail");
					*sensor_id = 0xFFFFFFFF;
					return ERROR_SENSOR_CONNECT_FAIL;
				}
			}
 /* N17 code for HQ-293327 by changqi at 2023/05/16 end */
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		pr_info("Read id fail,sensor id: 0x%x\n", *sensor_id);
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

	LOG_INF("[open]: PLATFORM:MT6737,MIPI 24LANE\n");
	LOG_INF("preview 1296*972@30fps,360Mbps/lane;"
		"capture 2592*1944@30fps,880Mbps/lane\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}

			retry--;
		} while(retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		LOG_INF("open sensor id fail: 0x%x\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
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
/* N17 code for HQ-293327 by changqi start*/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.vblank_convert = 2504; //for 3264x2448
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
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
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
    imgsensor.vblank_convert = 2504; //for 3264x2448
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
}    /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.vblank_convert = 2504; //for 3264x1836 30fps
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.vblank_convert = 2504;// for 3264X2448
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
    imgsensor.vblank_convert = 1252;//for 1632x1224 30fps
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

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.vblank_convert = 1252; //for 1632x1224
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	return ERROR_NONE;
}
/* N17 code for HQ-293327 by changqi end*/
static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
    imgsensor.vblank_convert = 1252; //for 1632X1224
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
    imgsensor.vblank_convert = 2504; //for 3264x2448
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	return ERROR_NONE;
}

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
    imgsensor.vblank_convert = 2504; //for 3264x2448
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
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
    imgsensor.vblank_convert = 2504; //for 3264x2448
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
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;
	return ERROR_NONE;
}    /*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
    sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
    sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
    sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 start */
    sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 end */
	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
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
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	    sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
	break;
	default:
	    sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
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
		LOG_INF("[odin]default mode\n");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}    /* control() */



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
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable,
			UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
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
		//Test
		//framerate = 1200;

	    frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
	    frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
			(frame_length - imgsensor_info.normal_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
				(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length - imgsensor_info.hs_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.hs_video.framelength + 	imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ?
			(frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ?
			(frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ?
			(frame_length - imgsensor_info.custom4.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ?
			(frame_length - imgsensor_info.custom5.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom5.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();
	break;
	default:  //coding with  preview scenario by default
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			//set_dummy();
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



static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if(enable)
	{
		write_cmos_sensor(0xfd,0x01);
		write_cmos_sensor(0x12,0x01);
	}
	else
	{
		write_cmos_sensor(0xfd,0x01);
		write_cmos_sensor(0x12,0x00);
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_control enable =%d\n", enable);
	if (enable){
		write_cmos_sensor(0xfd, 0x00);
		write_cmos_sensor(0xa0, 0x01);

	}else{
		write_cmos_sensor(0xfd, 0x00);
		write_cmos_sensor(0xa0, 0x00);
	}
	mdelay(10);

	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16=(UINT16 *) feature_para;
	UINT16 *feature_data_16=(UINT16 *) feature_para;
	UINT32 *feature_return_para_32=(UINT32 *) feature_para;
	UINT32 *feature_data_32=(UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;

	unsigned long long *feature_data =
		(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
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
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 start */
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 end */
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len=4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
	    *feature_return_para_32 = imgsensor.pclk;
	    *feature_para_len = 4;
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
		case MSDK_SCENARIO_ID_CUSTOM2:
			rate = imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			rate = imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			rate = imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			rate = imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
		    rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	}
	break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) *feature_data);
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
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len=4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
	    LOG_INF("current fps :%d\n", *feature_data_32);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.current_fps = (UINT16)*feature_data_32;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data_32;
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
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	    //LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
		//	(UINT16)*feature_data, (UINT16)*(feature_data+1),
		//	(UINT16)*(feature_data+2));

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
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 start */
		set_shutter_frame_length((UINT16) (*feature_data), (UINT16) (*(feature_data + 1)), (BOOL) (*(feature_data + 2)));
/* N17 code for HQ-293330 by xuyanfei at 2023/05/11 end*/
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

UINT32 OV08D10_SUNNY_ULTRA_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}
