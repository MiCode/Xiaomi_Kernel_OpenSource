/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 OV8856_ofilm_mipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS MT6795
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
/* #include <asm/system.h> */
/* #include <linux/xlog.h> */
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov8856_ofilm_mipiraw_Sensor.h"
#include <linux/slab.h>

#define PFX "OV8856"

//#define OV8856OTP

//#define LOG_WRN(format, args...) xlog_printk(ANDROID_LOG_WARN ,PFX, "[%S] " format, __FUNCTION__, ##args)
//#defineLOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO ,PFX, "[%s] " format, __FUNCTION__, ##args)
//#define LOG_DBG(format, args...) xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S] " format, __FUNCTION__, ##args)
#define LOG_INF(format, args...)    pr_err(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV8856_OFILM_SENSOR_ID,		//record sensor id defined in Kd_imgsensor.h

	.checksum_value = 0xb1893b4f,		//checksum value for Camera Auto Test

	.pre = {
		.pclk = 144000000,	/* record different mode's pclk */
		.linelength = 1932,	/* record different mode's linelength */
		.framelength = 2482,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 1632,	/* record different mode's width of grabwindow */
		.grabwindow_height = 1224,	/* record different mode's height of grabwindow */
		/*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 23,
		/*       following for GetDefaultFramerateByScenario()  */
		.mipi_pixel_rate = 289000000,
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 144000000,
		.linelength = 1932,
		.framelength = 2482,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 23,
		.mipi_pixel_rate = 289000000,
		.max_framerate = 300,
		},
	.cap1 = {		/* capture for PIP 24fps relative information, capture1 mode must use same framelength, linelength with Capture mode for shutter calculate */
		 .pclk = 144000000,
		 .linelength = 1932,
		 .framelength = 4964,	/* MODIFY FOR 15FPS FOR MT6735 //2556 */
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 3264,
		 .grabwindow_height = 2448,
		 .mipi_data_lp2hs_settle_dc = 23,
		 .mipi_pixel_rate = 289000000,
		 .max_framerate = 150,
		 },
	.normal_video = {
			 .pclk = 144000000,
			 .linelength = 1932,
			 .framelength = 2482,
			 .startx = 0,
			 .starty = 0,
			 .grabwindow_width = 3264,
			 .grabwindow_height = 2448,
			 .mipi_data_lp2hs_settle_dc = 23,
			 .mipi_pixel_rate = 289000000,
			 .max_framerate = 300,
			 },
	.hs_video = {
		     .pclk = 144000000,	/* record different mode's pclk */
		     .linelength = 1932,	/* record different mode's linelength */
		     .framelength = 620,	/* record different mode's framelength */
		     .startx = 0,	/* record different mode's startx of grabwindow */
		     .starty = 0,	/* record different mode's starty of grabwindow */
		     .grabwindow_width = 640,	/* record different mode's width of grabwindow */
		     .grabwindow_height = 480,	/* record different mode's height of grabwindow */
		     /*       following for  MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario  */
		     .mipi_data_lp2hs_settle_dc = 30,
		     /*       following for GetDefaultFramerateByScenario()  */
			 .mipi_pixel_rate = 289000000,
		     .max_framerate = 1200,
		     },
	.slim_video = {
		       .pclk = 144000000,	/* record different mode's pclk */
		       .linelength = 1932,	/* record different mode's linelength */
		       .framelength = 2482,	/* record different mode's framelength */
		       .startx = 0,	/* record different mode's startx of grabwindow */
		       .starty = 0,	/* record different mode's starty of grabwindow */
		       .grabwindow_width = 1632,	/* record different mode's width of grabwindow */
		       .grabwindow_height = 1224,	/* record different mode's height of grabwindow */
		       /*       following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
		       .mipi_data_lp2hs_settle_dc = 30,
		       /*       following for GetDefaultFramerateByScenario()  */
			   .mipi_pixel_rate = 289000000,
		       .max_framerate = 300,

		       },
	.margin = 6,			//sensor framelength & shutter margin
	.min_shutter = 6,		//min shutter
	.max_frame_length = 0x7fff,//max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,	//shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num ,don't support Slow motion

	.cap_delay_frame = 3,		//enter capture delay frame num
	.pre_delay_frame = 3,		//enter preview delay frame num
	.video_delay_frame = 3,		//enter video delay frame num
	.hs_video_delay_frame = 3,	//enter high speed video  delay frame num
	.slim_video_delay_frame = 3,//enter slim video delay frame num

	.isp_driving_current = ISP_DRIVING_8MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
   .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
   .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,//sensor output first pixel color
	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_4_LANE,//mipi lane num
	.i2c_addr_table = {0x20, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x4C00,					//current shutter
	.gain = 0x200,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 30,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,//record current sensor's i2c write id
	.current_ae_effective_frame = 1, //长曝光生效帧数。若N+1生效，值为1；N+2生效，值为2
};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {

{ 3296, 2480,	  0,	12, 3296, 2456, 1648, 1228,   8,	2, 1632, 1224,	 0, 0, 1632, 1224}, // Preview
{ 3296, 2480,    0,	12, 3296, 2456, 3296, 2456,   16,	4, 3264, 2448,	 0, 0, 3264, 2448}, // capture
{ 3296, 2480,    0,	12, 3296, 2456, 3296, 2456,   16,	4, 3264, 2448,	 0, 0, 3264, 2448}, // video
{ 3296, 2480,	  336,	272, 2624, 1936,  656,  484,   8,	2,  640,  480,	 0, 0,  640,  480}, //hight speed video
{ 3296, 2480,	  0,	12, 3296, 2456, 1648, 1228,   8,	2, 1632, 1224,	 0, 0, 1632, 1224}   // slim video
};
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);


	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0x0100, 0x01);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 get_byte = 0;

	get_byte = (read_cmos_sensor(0x300B) << 8) | read_cmos_sensor(0x300C);

	get_byte = get_byte + 1;

	return get_byte;
}
static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	//kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min framelength should be = %d?\n", framerate, min_framelength_en);

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
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

#if 0
static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	kal_uint32 frame_length = 0;

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	} else {
		// Extend frame length
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	write_cmos_sensor(0x3500, (shutter>>12) & 0x0F);
	write_cmos_sensor(0x3501, (shutter>>4) & 0xFF);
	write_cmos_sensor(0x3502, (shutter<<4) & 0xF0);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

	//LOG_INF("frame_length = %d ", frame_length);

}	/*	write_shutter  */
#endif

static bool bNeedSetNormalMode = KAL_FALSE;


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
	kal_uint16 realtime_fps = 0;
	unsigned int value_0303 = 0x01;
	unsigned int value_030f = 0x04;
	unsigned int value_0312 = 0x01;
	unsigned int  value_380c = 0x07;
	unsigned int value_380d = 0x8c;
	unsigned int value_380e = 0x09;
	unsigned int value_380f = 0xb2;
	unsigned int value_3500 = 0x00;
	unsigned int value_3501 = 0x9a;
	unsigned int value_3502 = 0x20;
	unsigned int value_4837 = 0x0c;

	//kal_uint32 frame_length = 0;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	//write_shutter(shutter);
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	if (shutter < 32000) {

		if (bNeedSetNormalMode) {
			LOG_INF("LZM 1111 exit long shutter = %d\n", shutter);
			write_cmos_sensor(0x0100, 0x00);
			write_cmos_sensor(0x0303, 0x01);
			write_cmos_sensor(0x030f, 0x04);
			write_cmos_sensor(0x0312, 0x01);
			write_cmos_sensor(0x380c, 0x07);
			write_cmos_sensor(0x380d, 0x8c);
			write_cmos_sensor(0x380e, 0x09);
			write_cmos_sensor(0x380f, 0xb2);
			write_cmos_sensor(0x3500, 0x00);
			write_cmos_sensor(0x3501, 0x9a);
			write_cmos_sensor(0x3502, 0x20);
			write_cmos_sensor(0x4837, 0x16);
			write_cmos_sensor(0x5780, 0x14);
			write_cmos_sensor(0x5782, 0x44);
			write_cmos_sensor(0x5783, 0x02);
			write_cmos_sensor(0x5789, 0x0f);
			write_cmos_sensor(0x578a, 0xfd);
			write_cmos_sensor(0x0100, 0x01);
			imgsensor.current_ae_effective_frame = 1;
			bNeedSetNormalMode = KAL_FALSE;
		}

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(0x0100, 0x00);
			write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0100, 0x01);
		}
	} else {
			// Extend frame length
			write_cmos_sensor(0x0100, 0x00);
			write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0100, 0x01);
	}
	// Update Shutter
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3502, (shutter << 4) & 0xFF);
	write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);
	write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);
	write_cmos_sensor(0x0100, 0x01);
	LOG_INF("Exit! shutter =%d, framelength =%d, realtime_fps =%d \n", shutter, imgsensor.frame_length, realtime_fps);
	} else{
		LOG_INF("ZT Enter Long Exposure shutter:%d ", shutter);
		bNeedSetNormalMode = KAL_TRUE;
		imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		imgsensor.current_ae_effective_frame = 1;
			switch (shutter) {
			case 37266:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X08;
				value_380d = 0XD0;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCA;
				value_3502 = 0XB0;
				value_4837 = 0X16;
				break;
			case 44719:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X0A;
				value_380d = 0X98;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XC7;
				value_3502 = 0X20;
				value_4837 = 0X16;
				break;
			case 59625:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X0E;
				value_380d = 0X18;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCB;
				value_3502 = 0X90;
				value_4837 = 0X16;
				break;
			case 74532:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X11;
				value_380d = 0XA0;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCA;
				value_3502 = 0XB0;
				value_4837 = 0X16;
				break;
			case 96892:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X16;
				value_380d = 0XE0;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCD;
				value_3502 = 0XF0;
				value_4837 = 0X16;
				break;
			case 119251:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X1C;
				value_380d = 0X28;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCD;
				value_3502 = 0XC0;
				value_4837 = 0X16;
				break;
			case 149064:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X23;
				value_380d = 0X30;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCE;
				value_3502 = 0X40;
				value_4837 = 0X16;
				break;
			case 186330:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X2B;
				value_380d = 0XF8;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCE;
				value_3502 = 0XF0;
				value_4837 = 0X16;
				break;
			case 238503:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X38;
				value_380d = 0X48;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCE;
				value_3502 = 0XE0;
				value_4837 = 0X16;
				break;
			case 298129:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X46;
				value_380d = 0X58;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCF;
				value_3502 = 0X20;
				value_4837 = 0X16;
				break;
			case 372661:
			    value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X57;
				value_380d = 0XF0;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCE;
				value_3502 = 0XF0;
				value_4837 = 0X16;
				break;
			case 447193:
			value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X69;
				value_380d = 0X80;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCF;
				value_3502 = 0X70;
				value_4837 = 0X16;
				break;
			case 596258:
			value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0X8C;
				value_380d = 0XA8;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCF;
				value_3502 = 0X90;
				value_4837 = 0X16;
				break;
			case 745323:
			value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0XAF;
				value_380d = 0XD0;
				value_380e = 0X7D;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0XCF;
				value_3502 = 0XA0;
				value_4837 = 0X16;
				break;
			case 968920:
				value_0303 = 0X01;
				value_030f = 0X04;
				value_0312 = 0X01;
				value_380c = 0Xe4;
				value_380d = 0X8f;
				value_380e = 0X7d;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0Xcf;
				value_3502 = 0Xa0;
				value_4837 = 0X16;
				break;
			case 1117984:
				value_0303 = 0X03;
				value_030f = 0X09;
				value_0312 = 0X03;
				value_380c = 0X83;
				value_380d = 0Xde;
				value_380e = 0X7d;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0Xcf;
				value_3502 = 0X90;
				value_4837 = 0X2c;
				break;
			case 1490646:
				value_0303 = 0X03;
				value_030f = 0X09;
				value_0312 = 0X03;
				value_380c = 0Xaf;
				value_380d = 0Xd2;
				value_380e = 0X7d;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0Xcf;
				value_3502 = 0X90;
				value_4837 = 0X2c;
				break;
			case 1863307:
				value_0303 = 0X03;
				value_030f = 0X09;
				value_0312 = 0X03;
				value_380c = 0Xdb;
				value_380d = 0Xc7;
				value_380e = 0X7d;
				value_380f = 0X00;
				value_3500 = 0X07;
				value_3501 = 0Xcf;
				value_3502 = 0X90;
				value_4837 = 0X2c;
				break;
			case 2235969:
				value_0303 = 0X03;
				value_030f = 0X09;
				value_0312 = 0X03;
				value_380c = 0XFF;
				value_380d = 0XFC;
				value_380e = 0X7F;
				value_380f = 0XFF;
				value_3500 = 0X07;
				value_3501 = 0XFF;
				value_3502 = 0X90;
				value_4837 = 0X2c;
				break;
			default:
			LOG_INF("LZM ERROR! Shutter = %d, does not match any values!", shutter);

	}

    write_cmos_sensor(0x0100, 0X00);
	write_cmos_sensor(0x3021, 0X23);
	write_cmos_sensor(0x0303, value_0303);
	write_cmos_sensor(0x030f, value_030f);
	write_cmos_sensor(0x0312, value_0312);
    write_cmos_sensor(0x3508, 0X00);
	write_cmos_sensor(0x3509, 0X80);
	write_cmos_sensor(0x380c, value_380c);
	write_cmos_sensor(0X380d, value_380d);
	write_cmos_sensor(0X380e, value_380e);
	write_cmos_sensor(0X380f, value_380f);
	write_cmos_sensor(0X3500, value_3500);
	write_cmos_sensor(0X3501, value_3501);
	write_cmos_sensor(0X3502, value_3502);
	write_cmos_sensor(0X4837, value_4837);
	write_cmos_sensor(0X5780, 0x3e);
	write_cmos_sensor(0X5782, 0x41);
	write_cmos_sensor(0X5783, 0x01);
	write_cmos_sensor(0X5789, 0xf5);
	write_cmos_sensor(0X578a, 0xf5);
	write_cmos_sensor(0X0100, 0x01);
	LOG_INF("ZT Exit Long Exposure shutter:%d 2020-04-20 ", shutter);
	}

}

#if 0
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	reg_gain = gain*2;
	//reg_gain = reg_gain & 0xFFFF;
	return (kal_uint16)reg_gain;
}
#endif
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

	LOG_INF("set_gain %d\n", gain);
	if (gain < BASEGAIN || gain > 15.5 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 15.5 * BASEGAIN)
			gain = 15 * BASEGAIN;
	}

	//reg_gain = gain2reg(gain);
	reg_gain = gain*2;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x3508, (reg_gain>>8));
	write_cmos_sensor(0x3509, (reg_gain&0xFF));
	return gain;
}	/*	set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
			if (le > imgsensor.min_frame_length - imgsensor_info.margin)
				imgsensor.frame_length = le + imgsensor_info.margin;
			else
				imgsensor.frame_length = imgsensor.min_frame_length;
			if (imgsensor.frame_length > imgsensor_info.max_frame_length)
				imgsensor.frame_length = imgsensor_info.max_frame_length;
			spin_unlock(&imgsensor_drv_lock);
      if (le < imgsensor_info.min_shutter)
      le = imgsensor_info.min_shutter;
			if (se < imgsensor_info.min_shutter)
      se = imgsensor_info.min_shutter;


		// Extend frame length first
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);

		write_cmos_sensor(0x3512, (se << 4) & 0xFF);
		write_cmos_sensor(0x3511, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3510, (se >> 12) & 0x0F);

		set_gain(gain);
	}

}


#if 1
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

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
			write_cmos_sensor(0x3820, ((read_cmos_sensor(0x3820) & 0xB9) | 0x00));
			write_cmos_sensor(0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
			write_cmos_sensor(0x502e, ((read_cmos_sensor(0x502e) & 0xFC) | 0x03));
			write_cmos_sensor(0x5001, ((read_cmos_sensor(0x5001) & 0xFB) | 0x00));
			write_cmos_sensor(0x5004, ((read_cmos_sensor(0x5004) & 0xFB) | 0x04));
			write_cmos_sensor(0x376b, ((read_cmos_sensor(0x376b) & 0xC0) | 0x30));
			break;
	case IMAGE_H_MIRROR:
			write_cmos_sensor(0x3820, ((read_cmos_sensor(0x3820) & 0xB9) | 0x00));
			write_cmos_sensor(0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
			write_cmos_sensor(0x502e, ((read_cmos_sensor(0x502e) & 0xFC) | 0x03));
			write_cmos_sensor(0x5001, ((read_cmos_sensor(0x5001) & 0xFB) | 0x00));
			write_cmos_sensor(0x5004, ((read_cmos_sensor(0x5004) & 0xFB) | 0x00));
			write_cmos_sensor(0x376b, ((read_cmos_sensor(0x376b) & 0xC0) | 0x30));
			break;
	case IMAGE_V_MIRROR:
			write_cmos_sensor(0x3820, ((read_cmos_sensor(0x3820) & 0xB9) | 0x46));
			write_cmos_sensor(0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
			write_cmos_sensor(0x502e, ((read_cmos_sensor(0x502e) & 0xFC) | 0x00));
			write_cmos_sensor(0x5001, ((read_cmos_sensor(0x5001) & 0xFB) | 0x04));
			write_cmos_sensor(0x5004, ((read_cmos_sensor(0x5004) & 0xFB) | 0x04));
			write_cmos_sensor(0x376b, ((read_cmos_sensor(0x376b) & 0xC0) | 0x36));
			break;
	case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x3820, ((read_cmos_sensor(0x3820) & 0xB9) | 0x46));
			write_cmos_sensor(0x3821, ((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
			write_cmos_sensor(0x502e, ((read_cmos_sensor(0x502e) & 0xFC) | 0x00));
			write_cmos_sensor(0x5001, ((read_cmos_sensor(0x5001) & 0xFB) | 0x04));
			write_cmos_sensor(0x5004, ((read_cmos_sensor(0x5004) & 0xFB) | 0x00));
			write_cmos_sensor(0x376b, ((read_cmos_sensor(0x376b) & 0xC0) | 0x36));
			break;
	default:
			LOG_INF("Error image_mirror setting\n");
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

static void sensor_init(void)
{
	LOG_INF("E\n");
	/* R1A_AM08a, Ken Cui@2015/9/11
	R1A_AM10, Nick_zhang@2015/10/28
	R1A_AM11, Ken Cui@2015/11/09
	R1A_AM11a, Ken_Cui
	;Xclk 24Mhz
	;pclk 144Mhz
	linelength = 1932(0x78C)
	framelength = 2482(0x9b2)
	grabwindow_width  = 1632
	grabwindow_height = 1224
	max_framerate = 300,
	mipi_data_lp2hs_settle_dc =23?
	mipi_datarate = 720; Mbps
   */
	write_cmos_sensor(0x0103, 0x01);
	write_cmos_sensor(0x0302, 0x3c);
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x031e, 0x0c);
	write_cmos_sensor(0x3000, 0x00);
	write_cmos_sensor(0x300e, 0x00);
	write_cmos_sensor(0x3010, 0x00);
	write_cmos_sensor(0x3015, 0x84);
	write_cmos_sensor(0x3018, 0x72);
    write_cmos_sensor(0x3021, 0x03);
	write_cmos_sensor(0x3033, 0x24);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0x4c);
	write_cmos_sensor(0x3502, 0xe0);
	write_cmos_sensor(0x3503, 0x08);
	write_cmos_sensor(0x3505, 0x83);
	write_cmos_sensor(0x3508, 0x01);
	write_cmos_sensor(0x3509, 0x80);
	write_cmos_sensor(0x350c, 0x00);
	write_cmos_sensor(0x350d, 0x80);
	write_cmos_sensor(0x350e, 0x04);
	write_cmos_sensor(0x350f, 0x00);
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x02);
	write_cmos_sensor(0x3512, 0x00);
	write_cmos_sensor(0x3600, 0x72);
	write_cmos_sensor(0x3601, 0x40);
	write_cmos_sensor(0x3602, 0x30);
	write_cmos_sensor(0x3610, 0xc5);
	write_cmos_sensor(0x3611, 0x58);
	write_cmos_sensor(0x3612, 0x5c);
	write_cmos_sensor(0x3613, 0xca);
	write_cmos_sensor(0x3614, 0x60);
	write_cmos_sensor(0x3628, 0xff);
	write_cmos_sensor(0x3629, 0xff);
	write_cmos_sensor(0x362a, 0xff);
	write_cmos_sensor(0x3633, 0x10);
	write_cmos_sensor(0x3634, 0x10);
	write_cmos_sensor(0x3635, 0x10);
	write_cmos_sensor(0x3636, 0x10);
	write_cmos_sensor(0x3663, 0x08);
	write_cmos_sensor(0x3669, 0x34);
	write_cmos_sensor(0x366e, 0x08);
	write_cmos_sensor(0x3706, 0x86);
	write_cmos_sensor(0x370b, 0x7e);
	write_cmos_sensor(0x3714, 0x27);
	write_cmos_sensor(0x3730, 0x12);
	write_cmos_sensor(0x3733, 0x10);
	write_cmos_sensor(0x3764, 0x00);
	write_cmos_sensor(0x3765, 0x00);
	write_cmos_sensor(0x3769, 0x62);
	write_cmos_sensor(0x376a, 0x2a);
	write_cmos_sensor(0x376b, 0x30);
	write_cmos_sensor(0x3780, 0x00);
	write_cmos_sensor(0x3781, 0x24);
	write_cmos_sensor(0x3782, 0x00);
	write_cmos_sensor(0x3783, 0x23);
	write_cmos_sensor(0x3798, 0x2f);
	write_cmos_sensor(0x37a1, 0x60);
	write_cmos_sensor(0x37a8, 0x6a);
	write_cmos_sensor(0x37ab, 0x3f);
	write_cmos_sensor(0x37c2, 0x14);
	write_cmos_sensor(0x37c3, 0xf1);
	write_cmos_sensor(0x37c9, 0x80);
	write_cmos_sensor(0x37cb, 0x16);
	write_cmos_sensor(0x37cc, 0x16);
	write_cmos_sensor(0x37cd, 0x16);
	write_cmos_sensor(0x37ce, 0x16);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0c);
	write_cmos_sensor(0x3804, 0x0c);
	write_cmos_sensor(0x3805, 0xdf);
	write_cmos_sensor(0x3806, 0x09);
	write_cmos_sensor(0x3807, 0xa3);
	write_cmos_sensor(0x3808, 0x06);
	write_cmos_sensor(0x3809, 0x60);
	write_cmos_sensor(0x380a, 0x04);
	write_cmos_sensor(0x380b, 0xc8);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x8c);
	write_cmos_sensor(0x380e, 0x09);//04
	write_cmos_sensor(0x380f, 0xb2);//de
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3811, 0x08);
	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x00);
	write_cmos_sensor(0x3817, 0x00);
	write_cmos_sensor(0x3818, 0x00);
	write_cmos_sensor(0x3819, 0x00);
	write_cmos_sensor(0x3820, 0x90);
	write_cmos_sensor(0x3821, 0x67);
	write_cmos_sensor(0x382a, 0x03);
	write_cmos_sensor(0x382b, 0x01);
	write_cmos_sensor(0x3830, 0x06);
	write_cmos_sensor(0x3836, 0x02);
	write_cmos_sensor(0x3837, 0x10);
	write_cmos_sensor(0x3862, 0x04);
	write_cmos_sensor(0x3863, 0x08);
	write_cmos_sensor(0x3cc0, 0x33);
	write_cmos_sensor(0x3d85, 0x17);
	write_cmos_sensor(0x3d8c, 0x73);
	write_cmos_sensor(0x3d8d, 0xde);
	write_cmos_sensor(0x4001, 0xe0);
	write_cmos_sensor(0x4003, 0x40);
	write_cmos_sensor(0x4008, 0x00);
	write_cmos_sensor(0x4009, 0x05);
    write_cmos_sensor(0x400a, 0x00);
    write_cmos_sensor(0x400b, 0x84);
	write_cmos_sensor(0x400f, 0x80);
	write_cmos_sensor(0x4010, 0xf0);
	write_cmos_sensor(0x4011, 0xff);
	write_cmos_sensor(0x4012, 0x02);
	write_cmos_sensor(0x4013, 0x01);
	write_cmos_sensor(0x4014, 0x01);
	write_cmos_sensor(0x4015, 0x01);
	write_cmos_sensor(0x4042, 0x00);
	write_cmos_sensor(0x4043, 0x80);
	write_cmos_sensor(0x4044, 0x00);
	write_cmos_sensor(0x4045, 0x80);
	write_cmos_sensor(0x4046, 0x00);
	write_cmos_sensor(0x4047, 0x80);
	write_cmos_sensor(0x4048, 0x00);
	write_cmos_sensor(0x4049, 0x80);
	write_cmos_sensor(0x4041, 0x03);
	write_cmos_sensor(0x404c, 0x20);
	write_cmos_sensor(0x404d, 0x00);
	write_cmos_sensor(0x404e, 0x20);
	write_cmos_sensor(0x4203, 0x80);
	write_cmos_sensor(0x4307, 0x30);
	write_cmos_sensor(0x4317, 0x00);
	write_cmos_sensor(0x4503, 0x08);
	write_cmos_sensor(0x4601, 0x80);
	write_cmos_sensor(0x4800, 0x44);
	write_cmos_sensor(0x4816, 0x53);
	write_cmos_sensor(0x481b, 0x58);
	write_cmos_sensor(0x481f, 0x27);
	write_cmos_sensor(0x4837, 0x16);
	write_cmos_sensor(0x483c, 0x0f);
	write_cmos_sensor(0x484b, 0x05);
	write_cmos_sensor(0x5000, 0x77);
	write_cmos_sensor(0x5001, 0x0a);
	write_cmos_sensor(0x5004, 0x04);
	write_cmos_sensor(0x502e, 0x03);
	write_cmos_sensor(0x5030, 0x41);
	write_cmos_sensor(0x5795, 0x00);
	write_cmos_sensor(0x5796, 0x10);
	write_cmos_sensor(0x5797, 0x10);
	write_cmos_sensor(0x5798, 0x73);
	write_cmos_sensor(0x5799, 0x73);
	write_cmos_sensor(0x579a, 0x00);
	write_cmos_sensor(0x579b, 0x28);
	write_cmos_sensor(0x579c, 0x00);
	write_cmos_sensor(0x579d, 0x16);
	write_cmos_sensor(0x579e, 0x06);
	write_cmos_sensor(0x579f, 0x20);
	write_cmos_sensor(0x57a0, 0x04);
	write_cmos_sensor(0x57a1, 0xa0);
	//;DPC setting
	write_cmos_sensor(0x5780, 0x14);
	write_cmos_sensor(0x5781, 0x0f);
	write_cmos_sensor(0x5782, 0x44);
	write_cmos_sensor(0x5783, 0x02);
	write_cmos_sensor(0x5784, 0x01);
	write_cmos_sensor(0x5785, 0x01);
	write_cmos_sensor(0x5786, 0x00);
	write_cmos_sensor(0x5787, 0x04);
	write_cmos_sensor(0x5788, 0x02);
	write_cmos_sensor(0x5789, 0x0f);
	write_cmos_sensor(0x578a, 0xfd);
	write_cmos_sensor(0x578b, 0xf5);
	write_cmos_sensor(0x578c, 0xf5);
	write_cmos_sensor(0x578d, 0x03);
	write_cmos_sensor(0x578e, 0x08);
	write_cmos_sensor(0x578f, 0x0c);
	write_cmos_sensor(0x5790, 0x08);
	write_cmos_sensor(0x5791, 0x04);
	write_cmos_sensor(0x5792, 0x00);
	write_cmos_sensor(0x5793, 0x52);
	write_cmos_sensor(0x5794, 0xa3);
	write_cmos_sensor(0x59f8, 0x3d);//AM11a
	//;
	write_cmos_sensor(0x5a08, 0x02);
	write_cmos_sensor(0x5b00, 0x02);
	write_cmos_sensor(0x5b01, 0x10);
	write_cmos_sensor(0x5b02, 0x03);
	write_cmos_sensor(0x5b03, 0xcf);
	write_cmos_sensor(0x5b05, 0x6c);
	write_cmos_sensor(0x5e00, 0x00);
	write_cmos_sensor(0x0100, 0x01);

}	/*	sensor_init  */


static void preview_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3501, 0x4c);
	write_cmos_sensor(0x3502, 0xe0);
	write_cmos_sensor(0x366e, 0x08);
	write_cmos_sensor(0x3714, 0x27);
	write_cmos_sensor(0x37c2, 0x14);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0c);
	write_cmos_sensor(0x3804, 0x0c);
	write_cmos_sensor(0x3805, 0xdf);
	write_cmos_sensor(0x3806, 0x09);
	write_cmos_sensor(0x3807, 0xa3);
	write_cmos_sensor(0x3808, 0x06);
	write_cmos_sensor(0x3809, 0x60);
	write_cmos_sensor(0x380a, 0x04);
	write_cmos_sensor(0x380b, 0xc8);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x8c);
	write_cmos_sensor(0x380e, 0x09); //0x04);
	write_cmos_sensor(0x380f, 0xb2); //0xde);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3811, 0x08);
	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3820, 0x90);  //90 ann
	write_cmos_sensor(0x3821, 0x67);   //67 ann
   //write_cmos_sensor(0x4502, 0x50);   //ann add
	write_cmos_sensor(0x382a, 0x03);
	write_cmos_sensor(0x4009, 0x05);
	write_cmos_sensor(0x4601, 0x80);
	write_cmos_sensor(0x5795, 0x00);
	write_cmos_sensor(0x5796, 0x10);
	write_cmos_sensor(0x5797, 0x10);
	write_cmos_sensor(0x5798, 0x73);
	write_cmos_sensor(0x5799, 0x73);
	write_cmos_sensor(0x579a, 0x00);
	write_cmos_sensor(0x579b, 0x28);
	write_cmos_sensor(0x579c, 0x00);
	write_cmos_sensor(0x579d, 0x16);
	write_cmos_sensor(0x579e, 0x06);
	write_cmos_sensor(0x579f, 0x20);
	write_cmos_sensor(0x57a0, 0x04);
	write_cmos_sensor(0x57a1, 0xa0);
	write_cmos_sensor(0x366d, 0x00);
	write_cmos_sensor(0x5003, 0xc8);
	write_cmos_sensor(0x5006, 0x00);
	write_cmos_sensor(0x5007, 0x00);
	write_cmos_sensor(0x5e10, 0xfc);
	write_cmos_sensor(0x0100, 0x01);
}	/*	preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
	if (currefps == 150) {
		write_cmos_sensor(0x0100, 0x00);
		write_cmos_sensor(0x3501, 0x9a);
		write_cmos_sensor(0x3502, 0x20);
		write_cmos_sensor(0x366e, 0x10);
		write_cmos_sensor(0x3714, 0x23);
		write_cmos_sensor(0x37c2, 0x04);
		write_cmos_sensor(0x3800, 0x00);
		write_cmos_sensor(0x3801, 0x00);
		write_cmos_sensor(0x3802, 0x00);
		write_cmos_sensor(0x3803, 0x0c);
		write_cmos_sensor(0x3804, 0x0c);
		write_cmos_sensor(0x3805, 0xdf);
		write_cmos_sensor(0x3806, 0x09);
		write_cmos_sensor(0x3807, 0xa3);
		write_cmos_sensor(0x3808, 0x0c);
		write_cmos_sensor(0x3809, 0xc0);
		write_cmos_sensor(0x380a, 0x09);
		write_cmos_sensor(0x380b, 0x90);
		write_cmos_sensor(0x380c, 0x07);
		write_cmos_sensor(0x380d, 0x8c);
		write_cmos_sensor(0x380e, 0x13);//;09
		write_cmos_sensor(0x380f, 0x64);//;b2
		write_cmos_sensor(0x3810, 0x00);
		write_cmos_sensor(0x3811, 0x10);
		write_cmos_sensor(0x3812, 0x00);
		write_cmos_sensor(0x3813, 0x04);
		write_cmos_sensor(0x3814, 0x01);
		write_cmos_sensor(0x3820, 0x80);
		write_cmos_sensor(0x3821, 0x46);
	//write_cmos_sensor(0x4502, 0x50);
		write_cmos_sensor(0x382a, 0x01);
		write_cmos_sensor(0x4009, 0x0b);
		write_cmos_sensor(0x4601, 0x80);
		write_cmos_sensor(0x5795, 0x02);
		write_cmos_sensor(0x5796, 0x20);
		write_cmos_sensor(0x5797, 0x20);
		write_cmos_sensor(0x5798, 0xd5);
		write_cmos_sensor(0x5799, 0xd5);
		write_cmos_sensor(0x579a, 0x00);
		write_cmos_sensor(0x579b, 0x50);
		write_cmos_sensor(0x579c, 0x00);
		write_cmos_sensor(0x579d, 0x2c);
		write_cmos_sensor(0x579e, 0x0c);
		write_cmos_sensor(0x579f, 0x40);
		write_cmos_sensor(0x57a0, 0x09);
		write_cmos_sensor(0x57a1, 0x40);
		write_cmos_sensor(0x366d, 0x00);
		write_cmos_sensor(0x5003, 0xc8);
		write_cmos_sensor(0x5006, 0x00);
		write_cmos_sensor(0x5007, 0x00);
		write_cmos_sensor(0x5e10, 0xfc);
		write_cmos_sensor(0x0100, 0x01);

	} else{
		write_cmos_sensor(0x0100, 0x00);
		write_cmos_sensor(0x3501, 0x9a);
		write_cmos_sensor(0x3502, 0x20);
		write_cmos_sensor(0x366e, 0x10);
		write_cmos_sensor(0x3714, 0x23);
		write_cmos_sensor(0x37c2, 0x04);
		write_cmos_sensor(0x3800, 0x00);
		write_cmos_sensor(0x3801, 0x00);
		write_cmos_sensor(0x3802, 0x00);
		write_cmos_sensor(0x3803, 0x0c);
		write_cmos_sensor(0x3804, 0x0c);
		write_cmos_sensor(0x3805, 0xdf);
		write_cmos_sensor(0x3806, 0x09);
		write_cmos_sensor(0x3807, 0xa3);
		write_cmos_sensor(0x3808, 0x0c);
		write_cmos_sensor(0x3809, 0xc0);
		write_cmos_sensor(0x380a, 0x09);
		write_cmos_sensor(0x380b, 0x90);
		write_cmos_sensor(0x380c, 0x07);
		write_cmos_sensor(0x380d, 0x8c);
		write_cmos_sensor(0x380e, 0x09);
		write_cmos_sensor(0x380f, 0xb2);
		write_cmos_sensor(0x3810, 0x00);
		write_cmos_sensor(0x3811, 0x10);
		write_cmos_sensor(0x3812, 0x00);
		write_cmos_sensor(0x3813, 0x04);
		write_cmos_sensor(0x3814, 0x01);
		write_cmos_sensor(0x3820, 0x80);
		write_cmos_sensor(0x3821, 0x46);
	   // write_cmos_sensor(0x4502, 0x50); //ann add
		write_cmos_sensor(0x382a, 0x01);
		write_cmos_sensor(0x4009, 0x0b);
		write_cmos_sensor(0x4601, 0x80);
		write_cmos_sensor(0x5795, 0x02);
		write_cmos_sensor(0x5796, 0x20);
		write_cmos_sensor(0x5797, 0x20);
		write_cmos_sensor(0x5798, 0xd5);
		write_cmos_sensor(0x5799, 0xd5);
		write_cmos_sensor(0x579a, 0x00);
		write_cmos_sensor(0x579b, 0x50);
		write_cmos_sensor(0x579c, 0x00);
		write_cmos_sensor(0x579d, 0x2c);
		write_cmos_sensor(0x579e, 0x0c);
		write_cmos_sensor(0x579f, 0x40);
		write_cmos_sensor(0x57a0, 0x09);
		write_cmos_sensor(0x57a1, 0x40);
		write_cmos_sensor(0x366d, 0x00);
		write_cmos_sensor(0x5003, 0xc8);
		write_cmos_sensor(0x5006, 0x00);
		write_cmos_sensor(0x5007, 0x00);
		write_cmos_sensor(0x5e10, 0xfc);
		write_cmos_sensor(0x0100, 0x01);
	}

}
static void vga_setting_120fps(void)
{
		write_cmos_sensor(0x0100, 0x00);

		write_cmos_sensor(0x3501, 0x25);
		write_cmos_sensor(0x3502, 0xc0);
		write_cmos_sensor(0x366e, 0x08);
		write_cmos_sensor(0x3714, 0x29);
		write_cmos_sensor(0x37c2, 0x34);
		write_cmos_sensor(0x3800, 0x01);
		write_cmos_sensor(0x3801, 0x50);
		write_cmos_sensor(0x3802, 0x01);
		write_cmos_sensor(0x3803, 0x10);
		write_cmos_sensor(0x3804, 0x0b);
		write_cmos_sensor(0x3805, 0x8f);
		write_cmos_sensor(0x3806, 0x08);
		write_cmos_sensor(0x3807, 0x9f);
		write_cmos_sensor(0x3808, 0x02);
		write_cmos_sensor(0x3809, 0x80);
		write_cmos_sensor(0x380a, 0x01);
		write_cmos_sensor(0x380b, 0xe0);
		write_cmos_sensor(0x380c, 0x07);
		write_cmos_sensor(0x380d, 0x8c);
		write_cmos_sensor(0x380e, 0x02);
		write_cmos_sensor(0x380f, 0x6c);
		write_cmos_sensor(0x3810, 0x00);
		write_cmos_sensor(0x3811, 0x07);
		write_cmos_sensor(0x3812, 0x00);
		write_cmos_sensor(0x3813, 0x02);
		write_cmos_sensor(0x3814, 0x03);
		write_cmos_sensor(0x3815, 0x01);
		write_cmos_sensor(0x3820, 0x90);
		write_cmos_sensor(0x3821, 0x67);
	//write_cmos_sensor(0x4502, 0x50);  //ann add
		write_cmos_sensor(0x382a, 0x07);
		write_cmos_sensor(0x4009, 0x05);
		write_cmos_sensor(0x4601, 0x40);
		write_cmos_sensor(0x5795, 0x00);
		write_cmos_sensor(0x5796, 0x10);
		write_cmos_sensor(0x5797, 0x10);
		write_cmos_sensor(0x5798, 0x73);
		write_cmos_sensor(0x5799, 0x73);
		write_cmos_sensor(0x579a, 0x00);
		write_cmos_sensor(0x579b, 0x00);
		write_cmos_sensor(0x579c, 0x00);
		write_cmos_sensor(0x579d, 0x00);
		write_cmos_sensor(0x579e, 0x05);
		write_cmos_sensor(0x579f, 0xa0);
		write_cmos_sensor(0x57a0, 0x03);
		write_cmos_sensor(0x57a1, 0x20);
		write_cmos_sensor(0x366d, 0x11);
		write_cmos_sensor(0x5003, 0xc0);
		write_cmos_sensor(0x5006, 0x02);
		write_cmos_sensor(0x5007, 0x90);
		write_cmos_sensor(0x5e10, 0x7c);

		write_cmos_sensor(0x0100, 0x01);

}
#if 0
static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
   capture_setting(currefps);
}
#endif
static void hs_video_setting(void)
{
	LOG_INF("E\n");
	vga_setting_120fps();

}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();
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
static kal_uint16 get_vendor_id(void)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(0x01 >> 8), (char)(0x01 & 0xFF) };
	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, 0xA8);
	return get_byte;

}
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint8 vendor_id = 0;

	vendor_id = get_vendor_id();
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
		//if (0x06 == vendor_id) {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("[8856_ultra]get imgsensor:imgsensor.i2c_write_id: 0x%x, *sensor_id=0x%x, vendor_id=0x%x\n",
				imgsensor.i2c_write_id, *sensor_id, vendor_id);
				return ERROR_NONE;

			}
		//}
	LOG_INF("Read sensor id fail,imgsensor.i2c_write_id: 0x%x, *sensor_id=0x%x\n", imgsensor.i2c_write_id, *sensor_id);
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


/*
#ifdef OV8856OTP

struct otp_struct {
int flag; // bit[7]: info, bit[6]:wb, bit[5]:vcm, bit[4]:lenc
int module_integrator_id;
int lens_id;
int production_year;
int production_month;
int production_day;
int rg_ratio;
int bg_ratio;
int lenc[240];
int checksum;
int VCM_start;
int VCM_end;
int VCM_dir;
};
//struct otp_struct otp_ptr;

#define RG_Ratio_Typical   0x12F
#define BG_Ratio_Typical   0x13F

static int read_otp(struct otp_struct *otp_ptr)
{
	int otp_flag, addr, temp, i;
	//set 0x5001[3] to ???0???\A1\EC?\A8\A8
	int temp1;
	int checksum2 = 0;

	temp1 = read_cmos_sensor(0x5001);
	write_cmos_sensor(0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	// read OTP into bufferwrite_cmos_sensor
	write_cmos_sensor(0x3d84, 0xC0);
	write_cmos_sensor(0x3d88, 0x70); // OTP start address
	write_cmos_sensor(0x3d89, 0x10);
	write_cmos_sensor(0x3d8A, 0x72); // OTP end address
	write_cmos_sensor(0x3d8B, 0x0a);
	write_cmos_sensor(0x3d81, 0x01); // load otp into buffer
	mdelay(10);
	// OTP base information and WB calibration data
	otp_flag = read_cmos_sensor(0x7010);
	addr = 0;
	if ((otp_flag & 0xc0) == 0x40) {
	addr = 0x7011; // base address of info group 1
	} else if ((otp_flag & 0x30) == 0x10) {
	addr = 0x7019; // base address of info group 2
	}  else
	if (addr != 0) {
	(*otp_ptr).flag = 0xC0; // valid info and AWB in OTP
	(*otp_ptr).module_integrator_id = read_cmos_sensor(addr);
	(*otp_ptr).lens_id = read_cmos_sensor(addr + 1);
	(*otp_ptr).production_year = read_cmos_sensor(addr + 2);
	(*otp_ptr).production_month = read_cmos_sensor(addr + 3);
	(*otp_ptr).production_day = read_cmos_sensor(addr + 4);
	temp = read_cmos_sensor(addr + 7);
	(*otp_ptr).rg_ratio = (read_cmos_sensor(addr + 5)<<2) + ((temp>>6) & 0x03);
	(*otp_ptr).bg_ratio = (read_cmos_sensor(addr + 6)<<2) + ((temp>>4) & 0x03);
	LOG_INF("module_integrator_id=%x ,lens_id=%x\n", (*otp_ptr).module_integrator_id, (*otp_ptr).lens_id);
	LOG_INF("rg_ratio=%x ,bg_ratio=%x\n", (*otp_ptr).rg_ratio, (*otp_ptr).bg_ratio);
	} else {
	(*otp_ptr).flag = 0x00; // not info and AWB in OTP
	(*otp_ptr).module_integrator_id = 0;
	(*otp_ptr).lens_id = 0;
	(*otp_ptr).production_year = 0;
	(*otp_ptr).production_month = 0;
	(*otp_ptr).production_day = 0;
	(*otp_ptr).rg_ratio = 0;
	(*otp_ptr).bg_ratio = 0;
	LOG_INF("invalid AWB in OTP\n");
	}
	// OTP VCM Calibration
	otp_flag = read_cmos_sensor(0x7021);
	addr = 0;
	if ((otp_flag & 0xc0) == 0x40) {
	addr = 0x7022; // base address of VCM Calibration group 1
	} else if ((otp_flag & 0x30) == 0x10) {
	addr = 0x7025; // base address of VCM Calibration group 2
	}
	if (addr != 0) {
	(*otp_ptr).flag |= 0x20;
	temp = read_cmos_sensor(addr + 2);
	(*otp_ptr).VCM_start = (read_cmos_sensor(addr)<<2) | ((temp>>6) & 0x03);
	(*otp_ptr).VCM_end = (read_cmos_sensor(addr + 1) << 2) | ((temp>>4) & 0x03);
	(*otp_ptr).VCM_dir = (temp>>2) & 0x03;
	//LOG_INF("(* otp_ptr).VCM_start=%x ,(* otp_ptr).VCM_end=%x,(* otp_ptr).VCM_dir=%x\n",(* otp_ptr).VCM_start,(* otp_ptr).VCM_end,(* otp_ptr).VCM_dir);
	} else {
	(*otp_ptr).VCM_start = 0;
	(*otp_ptr).VCM_end = 0;
	(*otp_ptr).VCM_dir = 0;
	LOG_INF("invalid VCM Calibration in OTP\n");
	}
	// OTP Lenc Calibration
	otp_flag = read_cmos_sensor(0x7028);
	addr = 0;

	if ((otp_flag & 0xc0) == 0x40) {
	addr = 0x7029; // base address of Lenc Calibration group 1
	} else if ((otp_flag & 0x30) == 0x10) {
	addr = 0x711a; // base address of Lenc Calibration group 2
	}
	if (addr != 0) {
	for (i = 0; i < 240; i++) {
	(*otp_ptr).lenc[i] = read_cmos_sensor(addr + i);
	checksum2 += (*otp_ptr).lenc[i];
	//LOG_INF("(* otp_ptr).lenc[%d]=%x ,checksum2=%x\n",i,(* otp_ptr).lenc[i],checksum2);
	}
	checksum2 = (checksum2)%255 + 1;
	(*otp_ptr).checksum = read_cmos_sensor(addr + 240);
	//LOG_INF("(* otp_ptr).checksum=%x\n",(* otp_ptr).checksum);
	if ((*otp_ptr).checksum == checksum2) {
	(*otp_ptr).flag |= 0x10;
	}
	} else {
	for (i = 0; i < 240; i++) {
	(*otp_ptr).lenc[i] = 0;
	}
	LOG_INF("invalid Lenc Calibration in OTP\n");
	}
	for (i = 0x7010; i <= 0x720a; i++) {
	write_cmos_sensor(i, 0); // clear OTP buffer, recommended use continuous write to accelarate
	}
	//set 0x5001[3] to ???1???\A1\EC?\A8\A8
	temp1 = read_cmos_sensor(0x5001);
	write_cmos_sensor(0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	return (*otp_ptr).flag;
}
*/
// return value:
// bit[7]: 0 no otp info, 1 valid otp info
// bit[6]: 0 no otp wb, 1 valib otp wb
// bit[5]: 0 no otp vcm, 1 valid otp vcm
// bit[4]: 0 no otp lenc, 1 valid otp lenc
/*
static int apply_otp(struct otp_struct *otp_ptr)
{
	int rg, bg, R_gain, G_gain, B_gain, Base_gain, temp, i;
	// apply OTP WB Calibration

	LOG_INF("rg_ratio=%x ,bg_ratio=%x\n", (*otp_ptr).rg_ratio, (*otp_ptr).bg_ratio);

	if ((*otp_ptr).flag & 0x40) {
	rg = (*otp_ptr).rg_ratio;
	bg = (*otp_ptr).bg_ratio;
	//calculate G gain
	R_gain = (RG_Ratio_Typical*1000) / rg;
	B_gain = (BG_Ratio_Typical*1000) / bg;
	G_gain = 1000;
	if (R_gain < 1000 || B_gain < 1000) {
	if (R_gain < B_gain)
		 Base_gain = R_gain;
	else
		Base_gain = B_gain;
	} else{
	 Base_gain = G_gain;
	}
	R_gain = 0x400 * R_gain / (Base_gain);
	B_gain = 0x400 * B_gain / (Base_gain);
	G_gain = 0x400 * G_gain / (Base_gain);
	// update sensor WB gain
	if (R_gain > 0x400) {
	write_cmos_sensor(0x5019, R_gain>>8);
	write_cmos_sensor(0x501a, R_gain & 0x00ff);
	}
	if (G_gain > 0x400) {
	write_cmos_sensor(0x501b, G_gain>>8);
	write_cmos_sensor(0x501c, G_gain & 0x00ff);
	}
	if (B_gain > 0x400) {
	write_cmos_sensor(0x501d, B_gain>>8);
	write_cmos_sensor(0x501e, B_gain & 0x00ff);
	}
	}
	// apply OTP Lenc Calibration
	if ((*otp_ptr).flag & 0x10) {
	temp = read_cmos_sensor(0x5000);
	temp = 0x20 | temp;
	write_cmos_sensor(0x5000, temp);
	for (i = 0; i < 240; i++) {
	write_cmos_sensor(0x5900 + i, (*otp_ptr).lenc[i]);
	//LOG_INF("(* otp_ptr).lenc[%d]=%x\n",i,(*otp_ptr).lenc[i]);
	}
	}
	return (*otp_ptr).flag;
}*/

//#endif
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
	//const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;
/*
#ifdef OV8856OTP
	struct otp_struct *otp_ptr = (struct otp_struct *)kzalloc(sizeof(struct otp_struct), GFP_KERNEL);
#endif
*/
	LOG_INF("PLATFORM:MT6595,MIPI 4LANE\n");
	//LOG_INF("preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n");

	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
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
			LOG_INF("Read sensor id fail, i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
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

	mdelay(10);
	/*
	#ifdef OV8856OTP
		LOG_INF("Apply the sensor OTP\n");
		read_otp(otp_ptr);
		//apply_otp(otp_ptr);
		kfree(otp_ptr);
	#endif
	*/
	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x2D00;
	imgsensor.gain = 0x100;
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
	LOG_INF("E\n");

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
	set_mirror_flip(IMAGE_NORMAL);
	mdelay(10);
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
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {//15fps
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n", imgsensor.current_fps, imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	mdelay(10);

	if (imgsensor.test_pattern == KAL_TRUE) {
		//write_cmos_sensor(0x5002,0x00);
		write_cmos_sensor(0x5000, (read_cmos_sensor(0x5000)&0xBF)|0x00);
 }
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	mdelay(10);

	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	mdelay(10);
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	mdelay(10);
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	//sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
	//sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
	//imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

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

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
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
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x
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
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
	default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



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
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static  kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if (framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		//case MSDK_SCENARIO_ID_CAMERA_ZSD:
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
	default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
			break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
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
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x5001, (read_cmos_sensor(0x5001)&0xF7)|0x00);
		write_cmos_sensor(0x5000, (read_cmos_sensor(0x5000)&0xDF)|0x00);
		write_cmos_sensor(0x5E00, 0x80);


	} else {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x5001, (read_cmos_sensor(0x5001)&0xF7)|0x08);
		write_cmos_sensor(0x5000, (read_cmos_sensor(0x5000)&0xDF)|0x20);
		write_cmos_sensor(0x5E00, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor(0x0100, 0X01);
	else
		write_cmos_sensor(0x0100, 0x00);
	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
			    UINT8 *feature_para, UINT32 *feature_para_len)
{
   UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
   UINT16 *feature_data_16 = (UINT16 *) feature_para;
   UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
   UINT32 *feature_data_32 = (UINT32 *) feature_para;
   unsigned long long *feature_data = (unsigned long long *) feature_para;
   //unsigned long long *feature_return_para=(unsigned long long *) feature_para;

   struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
   MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d, len=%d\n", feature_id, *feature_para_len);
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
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
	   *feature_return_para_32 = imgsensor.current_ae_effective_frame;
	    break;
     case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
	    memcpy(feature_return_para_32, &imgsensor.ae_frm_mode,
			 sizeof(struct IMGSENSOR_AE_FRM_MODE));
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
	    write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
	break;
	case SENSOR_FEATURE_GET_REGISTER:
	    sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
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
	   set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
	break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
	    set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (*(feature_data+1)));
	break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	    get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
	break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	    set_test_pattern_mode((BOOL)*feature_data);
	break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //  for factory mode auto testing
	    *feature_return_para_32 = imgsensor_info.checksum_value;
	   *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_FRAMERATE:
	    LOG_INF("current fps :%d\n", (UINT16)*feature_data_32);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.current_fps = (UINT16)*feature_data_32;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_SET_HDR:
	    LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.ihdr_en = (BOOL)*feature_data_32;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data_32);
	    wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	   LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
	   ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
	  break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
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


UINT32 OV8856_OFILM_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
