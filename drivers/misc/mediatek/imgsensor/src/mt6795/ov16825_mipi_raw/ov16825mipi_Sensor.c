/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 OV16825mipi_Sensor.c
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
//#include <asm/system.h>
#include <linux/xlog.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov16825mipi_Sensor.h"

/****************************Modify following Strings for debug****************************/
#define PFX "OV16825_camera_sensor"
#define LOG_1 LOG_INF("PLATFORM:MT6595, OV16825 MIPI 4LANE\n");
#define LOG_2 LOG_INF("preview 2304*1728@30fps,800Mbps/lane; video 2304*1728@30fps,800Mbps/lane; capture 16M@24fps,1096Mbps/lane\n");

/****************************   Modify end    *******************************************/
#define LOG_INF(format, args...)	xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static imgsensor_info_struct imgsensor_info = { 
	.sensor_id = OV16825MIPI_SENSOR_ID, /*sensor_id = 0x16820*/ //record sensor id defined in Kd_imgsensor.h
	
	.checksum_value = 0x17870f14, //checksum value for Camera Auto Test
	
	.pre = {
		.pclk = 80000000,											//record different mode's pclk
		.linelength = 1520,		/*OV16825 Note: linelength/4,it means line length per lane*/		//record different mode's linelength
		.framelength = 1754,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2304,		//record different mode's width of grabwindow
		.grabwindow_height = 1728,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 30,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
	.cap = {/*normal capture*/
		.pclk = 160000000,
		.linelength = 1910, /*OV16825 Note: linelength/4,it means line length per lane*/
		.framelength = 3490, //3490,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 30,//unit , ns
		.max_framerate = 240,
	},
	.cap1 = {/*PIP capture*/
		.pclk = 160000000,
		.linelength = 1910, /*OV16825 Note: linelength/4,it means line length per lane*/
		.framelength = 4188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 30,//unit , ns
		.max_framerate = 200, //less than 13M(include 13M),cap1 max framerate is 24fps,16M max framerate is 20fps, 20M max framerate is 15fps  
	},
	.normal_video = {
		.pclk = 80000000,
		.linelength = 1520,/*OV16825 Note: linelength/4,it means line length per lane*/
		.framelength = 1754,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2304,
		.grabwindow_height = 1728,
		.mipi_data_lp2hs_settle_dc = 30,//unit , ns
		.max_framerate = 300,
	},
	.hs_video = {/*slow motion*/
		.pclk = 160000000,
		.linelength = 1056,/*OV16825 Note: linelength/4,it means line length per lane*/
		.framelength = 1248,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 30,//unit , ns
		.max_framerate = 1200,
	},
	.slim_video = {/*VT Call*/
		.pclk = 80000000,
		.linelength = 496,/*OV16825 Note: linelength/4,it means line length per lane*/
		.framelength = 896,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 30,//unit , ns
		.max_framerate = 1800,
	},
	.margin = 4,			//sensor framelength & shutter margin
	.min_shutter = 1,		//min shutter
	.max_frame_length = 0x7fff,//max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,	//shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 3,		//enter capture delay frame num
	.pre_delay_frame = 3, 		//enter preview delay frame num
	.video_delay_frame = 3,		//enter video delay frame num
	.hs_video_delay_frame = 3,	//enter high speed video  delay frame num
	.slim_video_delay_frame = 3,//enter slim video delay frame num
	
	.isp_driving_current = ISP_DRIVING_6MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,//sensor output first pixel color
	.mclk = 24, //24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_4_LANE,//mipi lane num
	.i2c_addr_table = {0x6c, 0x20, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 240,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x6c,
};
  
/* Sensor output window information */
/*according toov16825 datasheet p53 image cropping*/
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{{ 4704, 3504,	  0,  12, 4672, 3468, 4672, 3468, 23, 2, 2304, 1728,	  0,	0, 2304, 1728}, // Preview 
 { 4704, 3504,	 32,  14, 4640, 3462, 4640, 3462, 15, 2, 4608, 3456,	  0,	0, 4608, 3456}, // capture 
 { 4704, 3504,	  0,  12, 4672, 3468, 4672, 3468, 23, 2, 2304, 1728,	  0,	0, 2304, 1728}, // video 
 { 4704, 3504,	384, 660, 3904, 2172, 3904, 2172, 23, 2, 1920, 1080,	  0,	0, 1920, 1080}, //hight speed video 
 { 4704, 3504, 1024,1020, 2656, 1464, 2656, 1464,  0, 0, 1280, 720 ,	  0,	0, 1280, 720}};// slim video 


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */

	/*  Add dummy pixels: */
    /* 0x380c [0:4], 0x380d defines the PCLKs in one line of OV16825  */  
    /* Add dummy lines:*/
    /* 0x380e [0:1], 0x380f defines total lines in one frame of OV16825 */

	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);	  
	write_cmos_sensor(0x380c, (imgsensor.line_length) >> 8);
	write_cmos_sensor(0x380d, (imgsensor.line_length) & 0xFF);
  
}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	//kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min framelength should enable? \n", framerate,min_framelength_en);
   
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length; 
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.frame_length;
	//if (dummy_line < 0)
	//	imgsensor.dummy_line = 0;
	//else
	//	imgsensor.dummy_line = dummy_line;
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
}	/*	set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	//kal_uint32 frame_length = 0;
	   
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
	
	/* OV Recommend Solution
	*  if shutter bigger than frame_length, should extend frame length first
	*/
	
	if(!shutter) shutter = 1; /*avoid 0*/
	
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)		
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	//if (shutter < imgsensor_info.min_shutter) shutter = imgsensor_info.min_shutter;
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) { 
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);	
		else {
		// Extend frame length
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	write_cmos_sensor(0x3502, (shutter << 4) & 0xFF);
	write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);	  
	write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);	
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

	//LOG_INF("frame_length = %d ", frame_length);
	
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


#if 0
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;
	
	reg_gain = ((gain / BASEGAIN) << 4) + ((gain % BASEGAIN) * 16 / BASEGAIN);
	reg_gain = reg_gain & 0xFFFF;
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
	kal_uint16 reg_gain = 0, i = 0;

	/*
	* sensor gain 1x = 128
	* max gain = 0x7ff = 15.992x <16x
	* here we just use 0x3508 analog gain 1 bit[3:2].
	* 16x~32x should use 0x3508 analog gain 0 bit[1:0]
	*/

	if (gain < BASEGAIN || gain >= 16 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain >= 16 * BASEGAIN)
			gain = 15.9 * BASEGAIN;		 
	}
 
	/*reg_gain = gain2reg(gain);*/
	

	gain *= 2;
	gain = (gain & 0x7ff);
		
	for(i = 1; i <= 3; i++){
		if(gain >= 0x100){
			reg_gain = reg_gain + 1;
			gain = gain/2;			
		}
	}

	reg_gain = (reg_gain << 2);

	/*下面这个if 其实不会跑进来，是因为iGain = (iGain & 0x7ff);  这里限制了最大不超过16x
	* if(iGain > 0x100){  
	*	gain_reg = gain_reg + 1;
	*	iGain = iGain/2;
	*	}
	*/
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain; 
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x3509, gain);
	write_cmos_sensor(0x3508, reg_gain);    
	
	return gain;
}	/*	set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("Warining:Do not supportIHDR, Return. le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
	return;
	
	if (imgsensor.ihdr_en) {
		
		spin_lock(&imgsensor_drv_lock);
			if (le > imgsensor.min_frame_length - imgsensor_info.margin)		
				imgsensor.frame_length = le + imgsensor_info.margin;
			else
				imgsensor.frame_length = imgsensor.min_frame_length;
			if (imgsensor.frame_length > imgsensor_info.max_frame_length)
				imgsensor.frame_length = imgsensor_info.max_frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
			if (se < imgsensor_info.min_shutter) se = imgsensor_info.min_shutter;
			
			
				// Extend frame length first
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


#if 0
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
			write_cmos_sensor(0x3820,((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
			write_cmos_sensor(0x3821,((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
			break;
		case IMAGE_H_MIRROR:
			write_cmos_sensor(0x3820,((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
			write_cmos_sensor(0x3821,((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
			break;
		case IMAGE_V_MIRROR:
			write_cmos_sensor(0x3820,((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
			write_cmos_sensor(0x3821,((read_cmos_sensor(0x3821) & 0xF9) | 0x06));		
			break;
		case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x3820,((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
			write_cmos_sensor(0x3821,((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
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

  
  /*
	  @@Initial - MIPI 4-Lane 4608x3456 10-bit 15fps_640Mbps_lane
	  100 99 4608 3456
	  100 98 1 1
	  102 81 0 ffff
	  102 84 1 ffff
	  102 3601 5dc
	  102 3f00 da2
	  102 910 31
	  */
	  //;Reset
	  write_cmos_sensor(0x0103, 0x01);
	  
	  //;delay 20ms
	  mDELAY(20);
	  
	  //;PLL
	  write_cmos_sensor(0x0100, 0x00);
	  write_cmos_sensor(0x0300, 0x02);
	  write_cmos_sensor(0x0302, 0x50);//;64
	  write_cmos_sensor(0x0305, 0x01);
	  write_cmos_sensor(0x0306, 0x00);
	  write_cmos_sensor(0x030b, 0x02);
	  write_cmos_sensor(0x030c, 0x14);
	  write_cmos_sensor(0x030e, 0x00);
	  write_cmos_sensor(0x0313, 0x02);
	  write_cmos_sensor(0x0314, 0x14);
	  write_cmos_sensor(0x031f, 0x00);
	  
	  write_cmos_sensor(0x3022, 0x01);
	  write_cmos_sensor(0x3032, 0x80);
	  write_cmos_sensor(0x3601, 0xf8);
	  write_cmos_sensor(0x3602, 0x00);
	  write_cmos_sensor(0x3605, 0x50);
	  write_cmos_sensor(0x3606, 0x00);
	  write_cmos_sensor(0x3607, 0x2b);
	  write_cmos_sensor(0x3608, 0x16);
	  write_cmos_sensor(0x3609, 0x00);
	  write_cmos_sensor(0x360e, 0x99);
	  write_cmos_sensor(0x360f, 0x75);
	  write_cmos_sensor(0x3610, 0x69);
	  write_cmos_sensor(0x3611, 0x59);
	  write_cmos_sensor(0x3612, 0x40);
	  write_cmos_sensor(0x3613, 0x89);
	  write_cmos_sensor(0x3615, 0x44);
	  write_cmos_sensor(0x3617, 0x00);
	  write_cmos_sensor(0x3618, 0x20);
	  write_cmos_sensor(0x3619, 0x00);
	  write_cmos_sensor(0x361a, 0x10);
	  write_cmos_sensor(0x361c, 0x10);
	  write_cmos_sensor(0x361d, 0x00);
	  write_cmos_sensor(0x361e, 0x00);
	  write_cmos_sensor(0x3640, 0x15);
	  write_cmos_sensor(0x3641, 0x54);
	  write_cmos_sensor(0x3642, 0x63);
	  write_cmos_sensor(0x3643, 0x32);
	  write_cmos_sensor(0x3644, 0x03);
	  write_cmos_sensor(0x3645, 0x04);
	  write_cmos_sensor(0x3646, 0x85);
	  write_cmos_sensor(0x364a, 0x07);
	  write_cmos_sensor(0x3707, 0x08);
	  write_cmos_sensor(0x3718, 0x75);
	  write_cmos_sensor(0x371a, 0x55);
	  write_cmos_sensor(0x371c, 0x55);
	  write_cmos_sensor(0x3733, 0x80);
	  write_cmos_sensor(0x3760, 0x00);
	  write_cmos_sensor(0x3761, 0x30);
	  write_cmos_sensor(0x3762, 0x00);
	  write_cmos_sensor(0x3763, 0xc0);
	  write_cmos_sensor(0x3764, 0x03);
	  write_cmos_sensor(0x3765, 0x00);
	  
	  write_cmos_sensor(0x3823, 0x08);
	  write_cmos_sensor(0x3827, 0x02);
	  write_cmos_sensor(0x3828, 0x00);
	  write_cmos_sensor(0x3832, 0x00);
	  write_cmos_sensor(0x3833, 0x00);
	  write_cmos_sensor(0x3834, 0x00);
	  write_cmos_sensor(0x3d85, 0x17);
	  write_cmos_sensor(0x3d8c, 0x70);
	  write_cmos_sensor(0x3d8d, 0xa0);
	  write_cmos_sensor(0x3f00, 0x02);
	  
	  write_cmos_sensor(0x4001, 0x83);
	  write_cmos_sensor(0x400e, 0x00);
	  write_cmos_sensor(0x4011, 0x00);
	  write_cmos_sensor(0x4012, 0x00);
	  write_cmos_sensor(0x4200, 0x08);
	  write_cmos_sensor(0x4302, 0x7f);
	  write_cmos_sensor(0x4303, 0xff);
	  write_cmos_sensor(0x4304, 0x00);
	  write_cmos_sensor(0x4305, 0x00);
	  write_cmos_sensor(0x4501, 0x30);
	  write_cmos_sensor(0x4603, 0x20);
	  write_cmos_sensor(0x4b00, 0x22);
	  write_cmos_sensor(0x4903, 0x00);
	  write_cmos_sensor(0x5000, 0x7f);
	  write_cmos_sensor(0x5001, 0x01);
	  write_cmos_sensor(0x5004, 0x00);
	  write_cmos_sensor(0x5013, 0x20);
	  write_cmos_sensor(0x5051, 0x00);
	  write_cmos_sensor(0x5500, 0x01);
	  write_cmos_sensor(0x5501, 0x00);
	  write_cmos_sensor(0x5502, 0x07);
	  write_cmos_sensor(0x5503, 0xff);
	  write_cmos_sensor(0x5505, 0x6c);
	  write_cmos_sensor(0x5509, 0x02);
	  write_cmos_sensor(0x5780, 0xfc);
	  write_cmos_sensor(0x5781, 0xff);
	  write_cmos_sensor(0x5787, 0x40);
	  write_cmos_sensor(0x5788, 0x08);
	  write_cmos_sensor(0x578a, 0x02);
	  write_cmos_sensor(0x578b, 0x01);
	  write_cmos_sensor(0x578c, 0x01);
	  write_cmos_sensor(0x578e, 0x02);
	  write_cmos_sensor(0x578f, 0x01);
	  write_cmos_sensor(0x5790, 0x01);
	  write_cmos_sensor(0x5792, 0x00);
	  write_cmos_sensor(0x5980, 0x00);
	  write_cmos_sensor(0x5981, 0x21);
	  write_cmos_sensor(0x5982, 0x00);
	  write_cmos_sensor(0x5983, 0x00);
	  write_cmos_sensor(0x5984, 0x00);
	  write_cmos_sensor(0x5985, 0x00);
	  write_cmos_sensor(0x5986, 0x00);
	  write_cmos_sensor(0x5987, 0x00);
	  write_cmos_sensor(0x5988, 0x00);
	  
	  //;Because current regsiter number in group hold is more than 85 (default), change group1 and group2 start address. Use group 0, 1, 2.
	  write_cmos_sensor(0x3201, 0x15);
	  write_cmos_sensor(0x3202, 0x2a);
  
	#if 1
	  //;MIPI 4-Lane 4608x3456 10-bit 15fps setting
	  
	  //;don't change any PLL VCO in group hold
	  write_cmos_sensor(0x0305, 0x01);
	  write_cmos_sensor(0x030e, 0x01);
	  
	  write_cmos_sensor(0x3018, 0x7a);
	  write_cmos_sensor(0x3031, 0x0a);
	  write_cmos_sensor(0x3603, 0x00);
	  write_cmos_sensor(0x3604, 0x00);
	  write_cmos_sensor(0x360a, 0x00);
	  write_cmos_sensor(0x360b, 0x02);
	  write_cmos_sensor(0x360c, 0x12);
	  write_cmos_sensor(0x360d, 0x00);
	  write_cmos_sensor(0x3614, 0x77);
	  write_cmos_sensor(0x3616, 0x30);
	  write_cmos_sensor(0x3631, 0x60);
	  write_cmos_sensor(0x3700, 0x30);
	  write_cmos_sensor(0x3701, 0x08);
	  write_cmos_sensor(0x3702, 0x11);
	  write_cmos_sensor(0x3703, 0x20);
	  write_cmos_sensor(0x3704, 0x08);
	  write_cmos_sensor(0x3705, 0x00);
	  write_cmos_sensor(0x3706, 0x84);
	  write_cmos_sensor(0x3708, 0x20);
	  write_cmos_sensor(0x3709, 0x3c);
	  write_cmos_sensor(0x370a, 0x01);
	  write_cmos_sensor(0x370b, 0x5d);
	  write_cmos_sensor(0x370c, 0x03);
	  write_cmos_sensor(0x370e, 0x20);
	  write_cmos_sensor(0x370f, 0x05);
	  write_cmos_sensor(0x3710, 0x20);
	  write_cmos_sensor(0x3711, 0x20);
	  write_cmos_sensor(0x3714, 0x31);
	  write_cmos_sensor(0x3719, 0x13);
	  write_cmos_sensor(0x371b, 0x03);
	  write_cmos_sensor(0x371d, 0x03);
	  write_cmos_sensor(0x371e, 0x09);
	  write_cmos_sensor(0x371f, 0x17);
	  write_cmos_sensor(0x3720, 0x0b);
	  write_cmos_sensor(0x3721, 0x18);
	  write_cmos_sensor(0x3722, 0x0b);
	  write_cmos_sensor(0x3723, 0x18);
	  write_cmos_sensor(0x3724, 0x04);
	  write_cmos_sensor(0x3725, 0x04);
	  write_cmos_sensor(0x3726, 0x02);
	  write_cmos_sensor(0x3727, 0x02);
	  write_cmos_sensor(0x3728, 0x02);
	  write_cmos_sensor(0x3729, 0x02);
	  write_cmos_sensor(0x372a, 0x25);
	  write_cmos_sensor(0x372b, 0x65);
	  write_cmos_sensor(0x372c, 0x55);
	  write_cmos_sensor(0x372d, 0x65);
	  write_cmos_sensor(0x372e, 0x53);
	  write_cmos_sensor(0x372f, 0x33);
	  write_cmos_sensor(0x3730, 0x33);
	  write_cmos_sensor(0x3731, 0x33);
	  write_cmos_sensor(0x3732, 0x03);
	  write_cmos_sensor(0x3734, 0x10);
	  write_cmos_sensor(0x3739, 0x03);
	  write_cmos_sensor(0x373a, 0x20);
	  write_cmos_sensor(0x373b, 0x0c);
	  write_cmos_sensor(0x373c, 0x1c);
	  write_cmos_sensor(0x373e, 0x0b);
	  write_cmos_sensor(0x373f, 0x80);
	  
	  write_cmos_sensor(0x3800, 0x00);
	  write_cmos_sensor(0x3801, 0x20);
	  write_cmos_sensor(0x3802, 0x00);
	  write_cmos_sensor(0x3803, 0x0e);
	  write_cmos_sensor(0x3804, 0x12);
	  write_cmos_sensor(0x3805, 0x3f);
	  write_cmos_sensor(0x3806, 0x0d);
	  write_cmos_sensor(0x3807, 0x93);
	  write_cmos_sensor(0x3808, 0x12);
	  write_cmos_sensor(0x3809, 0x00);
	  write_cmos_sensor(0x380a, 0x0d);
	  write_cmos_sensor(0x380b, 0x80);
	  write_cmos_sensor(0x380c, 0x05);
	  write_cmos_sensor(0x380d, 0xf8);
	  write_cmos_sensor(0x380e, 0x0d);
	  write_cmos_sensor(0x380f, 0xa2);
	  write_cmos_sensor(0x3811, 0x0f);
	  write_cmos_sensor(0x3813, 0x02);
	  write_cmos_sensor(0x3814, 0x01);
	  write_cmos_sensor(0x3815, 0x01);
	  write_cmos_sensor(0x3820, 0x00);
	  write_cmos_sensor(0x3821, 0x06);
	  write_cmos_sensor(0x3829, 0x00);
	  write_cmos_sensor(0x382a, 0x01);
	  write_cmos_sensor(0x382b, 0x01);
	  write_cmos_sensor(0x3830, 0x08);
	  
	  write_cmos_sensor(0x3f08, 0x20);
	  write_cmos_sensor(0x4000, 0xf1);	 // add for BCL trigger setting
	  write_cmos_sensor(0x4002, 0x04);
	  write_cmos_sensor(0x4003, 0x08);
	  write_cmos_sensor(0x4837, 0x14);
	  //write_cmos_sensor(0x4A00, 0x24);
	  
	  write_cmos_sensor(0x3501, 0xd9);
	  write_cmos_sensor(0x3502, 0xe0);
	  write_cmos_sensor(0x3508, 0x04);
	  write_cmos_sensor(0x3509, 0xff);
	  
	  write_cmos_sensor(0x3638, 0x00); //;activate 36xx
  
	#endif
	  
	  //write_cmos_sensor(0x3503, 0x00); //bit2,1:sensor gain, 0: real gain
  
	  write_cmos_sensor(0x0100, 0x01);
  
	  mDELAY(40);
    
}	/*	sensor_init  */


static void preview_setting(void)
{
	/*
	@@MIPI 4-Lane 2304x1728 10-bit VHbinning2 30fps 640Mbps/lane
	;;PCLK=HTS*VTS*fps=0x5f0*0x6da*30=1520*1754*30=80M
	100 99 2304 1728
	100 98 1 1
	102 81 0 ffff
	102 84 1 ffff
	102 3601 bb8
	102 3f00 6da
	
	; 2304x1728 4 lane	setting
	; Mipi: 4 lane
	; width		:2304 (0x900)
	; height		:1728 (0x6c0) 
	HTS = 0x5f0* 4 = 0x17c0 = 6080
	VTS = 0x6da = 1754
	
	*/
	//;Sensor Setting
	//;group 0
	
    write_cmos_sensor(0x0100, 0x00);
	
	write_cmos_sensor(0x3208, 0x00);
	
	write_cmos_sensor(0x301a, 0xfb);
	
	//;don't change any PLL VCO in group hold
	write_cmos_sensor(0x0305, 0x01);
	write_cmos_sensor(0x030e, 0x01);
	
	write_cmos_sensor(0x3018, 0x7a);
	write_cmos_sensor(0x3031, 0x0a);
	write_cmos_sensor(0x3603, 0x05);
	write_cmos_sensor(0x3604, 0x02);
	write_cmos_sensor(0x360a, 0x00);
	write_cmos_sensor(0x360b, 0x02);
	write_cmos_sensor(0x360c, 0x12);
	write_cmos_sensor(0x360d, 0x04);
	write_cmos_sensor(0x3614, 0x77);
	write_cmos_sensor(0x3616, 0x30);
	write_cmos_sensor(0x3631, 0x40);
	write_cmos_sensor(0x3700, 0x30);
	write_cmos_sensor(0x3701, 0x08);
	write_cmos_sensor(0x3702, 0x11);
	write_cmos_sensor(0x3703, 0x20);
	write_cmos_sensor(0x3704, 0x08);
	write_cmos_sensor(0x3705, 0x00);
	write_cmos_sensor(0x3706, 0x84);
	write_cmos_sensor(0x3708, 0x20);
	write_cmos_sensor(0x3709, 0x3c);
	write_cmos_sensor(0x370a, 0x01);
	write_cmos_sensor(0x370b, 0x5d);
	write_cmos_sensor(0x370c, 0x03);
	write_cmos_sensor(0x370e, 0x20);
	write_cmos_sensor(0x370f, 0x05);
	write_cmos_sensor(0x3710, 0x20);
	write_cmos_sensor(0x3711, 0x20);
	write_cmos_sensor(0x3714, 0x31);
	write_cmos_sensor(0x3719, 0x13);
	write_cmos_sensor(0x371b, 0x03);
	write_cmos_sensor(0x371d, 0x03);
	write_cmos_sensor(0x371e, 0x09);
	write_cmos_sensor(0x371f, 0x17);
	write_cmos_sensor(0x3720, 0x0b);
	write_cmos_sensor(0x3721, 0x18);
	write_cmos_sensor(0x3722, 0x0b);
	write_cmos_sensor(0x3723, 0x18);
	write_cmos_sensor(0x3724, 0x04);
	write_cmos_sensor(0x3725, 0x04);
	write_cmos_sensor(0x3726, 0x02);
	write_cmos_sensor(0x3727, 0x02);
	write_cmos_sensor(0x3728, 0x02);
	write_cmos_sensor(0x3729, 0x02);
	write_cmos_sensor(0x372a, 0x25);
	write_cmos_sensor(0x372b, 0x65);
	write_cmos_sensor(0x372c, 0x55);
	write_cmos_sensor(0x372d, 0x65);
	write_cmos_sensor(0x372e, 0x53);
	write_cmos_sensor(0x372f, 0x33);
	write_cmos_sensor(0x3730, 0x33);
	write_cmos_sensor(0x3731, 0x33);
	write_cmos_sensor(0x3732, 0x03);
	write_cmos_sensor(0x3734, 0x10);
	write_cmos_sensor(0x3739, 0x03);
	write_cmos_sensor(0x373a, 0x20);
	write_cmos_sensor(0x373b, 0x0c);
	write_cmos_sensor(0x373c, 0x1c);
	write_cmos_sensor(0x373e, 0x0b);
	write_cmos_sensor(0x373f, 0x80);
	
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0c);
	write_cmos_sensor(0x3804, 0x12);
	write_cmos_sensor(0x3805, 0x3f);
	write_cmos_sensor(0x3806, 0x0d);
	write_cmos_sensor(0x3807, 0x97);
	write_cmos_sensor(0x3808, 0x09);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x06);
	write_cmos_sensor(0x380b, 0xc0);
	write_cmos_sensor(0x380c, 0x05);
	write_cmos_sensor(0x380d, 0xf0);
	write_cmos_sensor(0x380e, 0x06);
	write_cmos_sensor(0x380f, 0xda);
	write_cmos_sensor(0x3811, 0x17);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3820, 0x01);
	write_cmos_sensor(0x3821, 0x07);
	write_cmos_sensor(0x3829, 0x02);
	write_cmos_sensor(0x382a, 0x03);
	write_cmos_sensor(0x382b, 0x01);
	write_cmos_sensor(0x3830, 0x08);
	
	write_cmos_sensor(0x3f08, 0x20);
	write_cmos_sensor(0x4002, 0x02);
	write_cmos_sensor(0x4003, 0x04);
	write_cmos_sensor(0x4837, 0x14);
	
	write_cmos_sensor(0x3501, 0x6d);
	write_cmos_sensor(0x3502, 0x60);
	write_cmos_sensor(0x3508, 0x08);
	write_cmos_sensor(0x3509, 0xff);
	
	write_cmos_sensor(0x3638, 0x00); //;activate 36xx
	
	write_cmos_sensor(0x301a, 0xf0);
	
	write_cmos_sensor(0x3208, 0x10);
	write_cmos_sensor(0x3208, 0xa0);

	
    write_cmos_sensor(0x0100, 0x01);
	
    //write_cmos_sensor(0x4800, 0x14);
	//write_cmos_sensor(0x5040, 0x80);

mDELAY(60);
}	/*	preview_setting  */

static void capture_setting(kal_uint16 curretfps)
{
	LOG_INF("E! currefps:%d\n",curretfps);
	/*20fps setting  for PIP
	*  24fps setting for normal capture
	*/

	if(200==curretfps){
	//if(1){
			/*
	 * @@ 0 20 MIPI 4-Lane 4608x3456 10-bit 24fps 1096Mbps/lane
	 * ;;PCLK=HTS*VTS*fps=0x776*0x105c*20=1910*4188*20=160M
	 * 100 99 4608 3456
	 * 100 98 1 1
	 * 102 81 0 ffff
	 * 102 84 1 ffff
	 * 102 3601 960
	 * 102 3f00 105c
	 */ 																
	/* Sensor Setting */
	write_cmos_sensor(0x0100, 0x00);
	/* ;sl 100 100 */
	mDELAY(100);
	
	write_cmos_sensor(0x0302, 0x89);
	
	/*don't change any PLL VCO in group hold*/
	write_cmos_sensor(0x0305, 0x01);
	write_cmos_sensor(0x030e, 0x00);
	
	write_cmos_sensor(0x3018, 0x7a);
	write_cmos_sensor(0x3031, 0x0a);
	write_cmos_sensor(0x3603, 0x00);
	write_cmos_sensor(0x3604, 0x00);
	write_cmos_sensor(0x360a, 0x00);
	write_cmos_sensor(0x360b, 0x82);
	write_cmos_sensor(0x360c, 0x1a);
	write_cmos_sensor(0x360d, 0x00);
	write_cmos_sensor(0x3614, 0x77);
	write_cmos_sensor(0x3616, 0x30);
	write_cmos_sensor(0x3631, 0x60);
	write_cmos_sensor(0x3700, 0x60);
	write_cmos_sensor(0x3701, 0x10);
	write_cmos_sensor(0x3702, 0x22);
	write_cmos_sensor(0x3703, 0x40);
	write_cmos_sensor(0x3704, 0x10);
	write_cmos_sensor(0x3705, 0x01);
	write_cmos_sensor(0x3706, 0x34);
	write_cmos_sensor(0x3708, 0x40);
	write_cmos_sensor(0x3709, 0x78);
	write_cmos_sensor(0x370a, 0x02);
	write_cmos_sensor(0x370b, 0xde);
	write_cmos_sensor(0x370c, 0x06);
	write_cmos_sensor(0x370e, 0x40);
	write_cmos_sensor(0x370f, 0x0a);
	write_cmos_sensor(0x3710, 0x30);
	write_cmos_sensor(0x3711, 0x40);
	write_cmos_sensor(0x3714, 0x31);
	write_cmos_sensor(0x3719, 0x25);
	write_cmos_sensor(0x371b, 0x05);
	write_cmos_sensor(0x371d, 0x05);
	write_cmos_sensor(0x371e, 0x11);
	write_cmos_sensor(0x371f, 0x2d);
	write_cmos_sensor(0x3720, 0x15);
	write_cmos_sensor(0x3721, 0x30);
	write_cmos_sensor(0x3722, 0x15);
	write_cmos_sensor(0x3723, 0x30);
	write_cmos_sensor(0x3724, 0x08);
	write_cmos_sensor(0x3725, 0x08);
	write_cmos_sensor(0x3726, 0x04);
	write_cmos_sensor(0x3727, 0x04);
	write_cmos_sensor(0x3728, 0x04);
	write_cmos_sensor(0x3729, 0x04);
	write_cmos_sensor(0x372a, 0x29);
	write_cmos_sensor(0x372b, 0xc9);
	write_cmos_sensor(0x372c, 0xa9);
	write_cmos_sensor(0x372d, 0xb9);
	write_cmos_sensor(0x372e, 0x95);
	write_cmos_sensor(0x372f, 0x55);
	write_cmos_sensor(0x3730, 0x55);
	write_cmos_sensor(0x3731, 0x55);
	write_cmos_sensor(0x3732, 0x05);
	write_cmos_sensor(0x3734, 0x90);
	write_cmos_sensor(0x3739, 0x05);
	write_cmos_sensor(0x373a, 0x40);
	write_cmos_sensor(0x373b, 0x18);
	write_cmos_sensor(0x373c, 0x38);
	write_cmos_sensor(0x373e, 0x15);
	write_cmos_sensor(0x373f, 0x80);
	
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x20);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0e);
	write_cmos_sensor(0x3804, 0x12);
	write_cmos_sensor(0x3805, 0x3f);
	write_cmos_sensor(0x3806, 0x0d);
	write_cmos_sensor(0x3807, 0x93);
	write_cmos_sensor(0x3808, 0x12);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x0d);
	write_cmos_sensor(0x380b, 0x80);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x76);
	write_cmos_sensor(0x380e, 0x10);
	write_cmos_sensor(0x380f, 0x5c);
	write_cmos_sensor(0x3811, 0x0f);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3820, 0x00);
	write_cmos_sensor(0x3821, 0x06);
	write_cmos_sensor(0x3829, 0x00);
	write_cmos_sensor(0x382a, 0x01);
	write_cmos_sensor(0x382b, 0x01);
	write_cmos_sensor(0x3830, 0x08);
	
	write_cmos_sensor(0x3f00, 0x02);
	write_cmos_sensor(0x3f02, 0x00);
	write_cmos_sensor(0x3f04, 0x00);
	write_cmos_sensor(0x3f05, 0x00);
	write_cmos_sensor(0x3f08, 0x40);
	write_cmos_sensor(0x4002, 0x04);
	write_cmos_sensor(0x4003, 0x08);
	write_cmos_sensor(0x4306, 0x00);
	write_cmos_sensor(0x4837, 0x0e);
	
	write_cmos_sensor(0x3501, 0xd9);
	write_cmos_sensor(0x3502, 0xe0);
	write_cmos_sensor(0x3508, 0x08);
	write_cmos_sensor(0x3509, 0xff);
	
	write_cmos_sensor(0x3638, 0x00); /* ;activate 36xx */
	
	write_cmos_sensor(0x0100, 0x01);

		}
	else{
	/*
	 * @@ 0 20 MIPI 4-Lane 4608x3456 10-bit 24fps 1096Mbps/lane
	 * ;;PCLK=HTS*VTS*fps=0x776*0xda2*24=1910*3490*24=160M
	 * 100 99 4608 3456
	 * 100 98 1 1
	 * 102 81 0 ffff
	 * 102 84 1 ffff
	 * 102 3601 960
	 * 102 3f00 da2
	 */ 																
	/* Sensor Setting */
	write_cmos_sensor(0x0100, 0x00);
	/* ;sl 100 100 */
	mDELAY(100);
	
	write_cmos_sensor(0x0302, 0x89);
	
	/*don't change any PLL VCO in group hold*/
	write_cmos_sensor(0x0305, 0x01);
	write_cmos_sensor(0x030e, 0x00);
	
	write_cmos_sensor(0x3018, 0x7a);
	write_cmos_sensor(0x3031, 0x0a);
	write_cmos_sensor(0x3603, 0x00);
	write_cmos_sensor(0x3604, 0x00);
	write_cmos_sensor(0x360a, 0x00);
	write_cmos_sensor(0x360b, 0x82);
	write_cmos_sensor(0x360c, 0x1a);
	write_cmos_sensor(0x360d, 0x00);
	write_cmos_sensor(0x3614, 0x77);
	write_cmos_sensor(0x3616, 0x30);
	write_cmos_sensor(0x3631, 0x60);
	write_cmos_sensor(0x3700, 0x60);
	write_cmos_sensor(0x3701, 0x10);
	write_cmos_sensor(0x3702, 0x22);
	write_cmos_sensor(0x3703, 0x40);
	write_cmos_sensor(0x3704, 0x10);
	write_cmos_sensor(0x3705, 0x01);
	write_cmos_sensor(0x3706, 0x34);
	write_cmos_sensor(0x3708, 0x40);
	write_cmos_sensor(0x3709, 0x78);
	write_cmos_sensor(0x370a, 0x02);
	write_cmos_sensor(0x370b, 0xde);
	write_cmos_sensor(0x370c, 0x06);
	write_cmos_sensor(0x370e, 0x40);
	write_cmos_sensor(0x370f, 0x0a);
	write_cmos_sensor(0x3710, 0x30);
	write_cmos_sensor(0x3711, 0x40);
	write_cmos_sensor(0x3714, 0x31);
	write_cmos_sensor(0x3719, 0x25);
	write_cmos_sensor(0x371b, 0x05);
	write_cmos_sensor(0x371d, 0x05);
	write_cmos_sensor(0x371e, 0x11);
	write_cmos_sensor(0x371f, 0x2d);
	write_cmos_sensor(0x3720, 0x15);
	write_cmos_sensor(0x3721, 0x30);
	write_cmos_sensor(0x3722, 0x15);
	write_cmos_sensor(0x3723, 0x30);
	write_cmos_sensor(0x3724, 0x08);
	write_cmos_sensor(0x3725, 0x08);
	write_cmos_sensor(0x3726, 0x04);
	write_cmos_sensor(0x3727, 0x04);
	write_cmos_sensor(0x3728, 0x04);
	write_cmos_sensor(0x3729, 0x04);
	write_cmos_sensor(0x372a, 0x29);
	write_cmos_sensor(0x372b, 0xc9);
	write_cmos_sensor(0x372c, 0xa9);
	write_cmos_sensor(0x372d, 0xb9);
	write_cmos_sensor(0x372e, 0x95);
	write_cmos_sensor(0x372f, 0x55);
	write_cmos_sensor(0x3730, 0x55);
	write_cmos_sensor(0x3731, 0x55);
	write_cmos_sensor(0x3732, 0x05);
	write_cmos_sensor(0x3734, 0x90);
	write_cmos_sensor(0x3739, 0x05);
	write_cmos_sensor(0x373a, 0x40);
	write_cmos_sensor(0x373b, 0x18);
	write_cmos_sensor(0x373c, 0x38);
	write_cmos_sensor(0x373e, 0x15);
	write_cmos_sensor(0x373f, 0x80);
	
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x20);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0e);
	write_cmos_sensor(0x3804, 0x12);
	write_cmos_sensor(0x3805, 0x3f);
	write_cmos_sensor(0x3806, 0x0d);
	write_cmos_sensor(0x3807, 0x93);
	write_cmos_sensor(0x3808, 0x12);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x0d);
	write_cmos_sensor(0x380b, 0x80);
	write_cmos_sensor(0x380c, 0x07);
	write_cmos_sensor(0x380d, 0x76);
	write_cmos_sensor(0x380e, 0x0d);
	write_cmos_sensor(0x380f, 0xa2);
	write_cmos_sensor(0x3811, 0x0f);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3820, 0x00);
	write_cmos_sensor(0x3821, 0x06);
	write_cmos_sensor(0x3829, 0x00);
	write_cmos_sensor(0x382a, 0x01);
	write_cmos_sensor(0x382b, 0x01);
	write_cmos_sensor(0x3830, 0x08);
	
	write_cmos_sensor(0x3f00, 0x02);
	write_cmos_sensor(0x3f02, 0x00);
	write_cmos_sensor(0x3f04, 0x00);
	write_cmos_sensor(0x3f05, 0x00);
	write_cmos_sensor(0x3f08, 0x40);
	write_cmos_sensor(0x4002, 0x04);
	write_cmos_sensor(0x4003, 0x08);
	write_cmos_sensor(0x4306, 0x00);
	write_cmos_sensor(0x4837, 0x0e);
	
	write_cmos_sensor(0x3501, 0xd9);
	write_cmos_sensor(0x3502, 0xe0);
	write_cmos_sensor(0x3508, 0x08);
	write_cmos_sensor(0x3509, 0xff);
	
	write_cmos_sensor(0x3638, 0x00); /* ;activate 36xx */
	
	write_cmos_sensor(0x0100, 0x01);
	
		}
	mDELAY(40);
			
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	
	preview_setting();
		
}
static void slim_video_setting(void)
{
	LOG_INF("OV16825-180fps\n");

	/*	
		@@ MIPI 4-Lane 1920x1080 121fps - reduce HTS then add VTS
		;10-bit Vskip2 Hbinning2 121fps 800Mbps/lane
		;;PCLK=HTS*VTS*fps=0x1f0*0x380*180=496*896*180=80M
		100 99 1920 1080
		100 98 1 1
		102 81 0 ffff
		102 84 1 ffff
		102 3601 2ee0
		102 3f00 452
		
		;1280x720 4 lane			setting
		 ; Mipi: 4 lane
		 ; width				  :1280 (0x500)
		 ; height				  :720	(0x2d0)
		 ;FPS=180.1
		 ;tLine=6.2us
		 ;HTS = 0x1f0 = 496
		 ;VTS = 0x380 = 896
		 ;v-blanking=176 (176x6.2=1091.2us=1.091ms) 
		*/
	
	 
	
		write_cmos_sensor(0x0100,0x00);
		//;Sensor Setting
		//;group 0
		write_cmos_sensor(0x3208,0x00);
		write_cmos_sensor(0x301a,0xfb);
		//don't change any PLL VCO in group hold
		write_cmos_sensor(0x0305,0x01);
		write_cmos_sensor(0x030e,0x01);
		write_cmos_sensor(0x3018,0x7a);
		write_cmos_sensor(0x3031,0x0a);
		write_cmos_sensor(0x3603,0x06);
		write_cmos_sensor(0x3604,0x02);
		write_cmos_sensor(0x360a,0x06);
		write_cmos_sensor(0x360b,0x02);
		write_cmos_sensor(0x360c,0x18);
		write_cmos_sensor(0x360d,0x04);
		write_cmos_sensor(0x3614,0x97);
		write_cmos_sensor(0x3616,0x33);
		write_cmos_sensor(0x3631,0x60);
		write_cmos_sensor(0x3700,0x30);
		write_cmos_sensor(0x3701,0x08);
		write_cmos_sensor(0x3702,0x11);
		write_cmos_sensor(0x3703,0x20);
		write_cmos_sensor(0x3704,0x08);
		write_cmos_sensor(0x3705,0x00);
		write_cmos_sensor(0x3706,0x84);
		write_cmos_sensor(0x3708,0x20);
		write_cmos_sensor(0x3709,0x3c);
		write_cmos_sensor(0x370a,0x01);
		write_cmos_sensor(0x370b,0x5d);
		write_cmos_sensor(0x370c,0x03);
		write_cmos_sensor(0x370e,0x20);
		write_cmos_sensor(0x370f,0x05);
		write_cmos_sensor(0x3710,0x20);
		write_cmos_sensor(0x3711,0x20);
		write_cmos_sensor(0x3714,0x31);
		write_cmos_sensor(0x3719,0x13);
		write_cmos_sensor(0x371b,0x03);
		write_cmos_sensor(0x371d,0x03);
		write_cmos_sensor(0x371e,0x09);
		write_cmos_sensor(0x371f,0x17);
		write_cmos_sensor(0x3720,0x0b);
		write_cmos_sensor(0x3721,0x18);
		write_cmos_sensor(0x3722,0x0b);
		write_cmos_sensor(0x3723,0x18);
		write_cmos_sensor(0x3724,0x04);
		write_cmos_sensor(0x3725,0x04);
		write_cmos_sensor(0x3726,0x02);
		write_cmos_sensor(0x3727,0x02);
		write_cmos_sensor(0x3728,0x02);
		write_cmos_sensor(0x3729,0x02);
		write_cmos_sensor(0x372a,0x25);
		write_cmos_sensor(0x372b,0x65);
		write_cmos_sensor(0x372c,0x95);
		write_cmos_sensor(0x372d,0x65);
		write_cmos_sensor(0x372e,0x53);
		write_cmos_sensor(0x372f,0x33);
		write_cmos_sensor(0x3730,0x33);
		write_cmos_sensor(0x3731,0x33);
		write_cmos_sensor(0x3732,0x03);
		write_cmos_sensor(0x3734,0x10);
		write_cmos_sensor(0x3739,0x03);
		write_cmos_sensor(0x373a,0x20);
		write_cmos_sensor(0x373b,0x0c);
		write_cmos_sensor(0x373c,0x1c);
		write_cmos_sensor(0x373e,0x0b);
		write_cmos_sensor(0x373f,0x80);
		write_cmos_sensor(0x3800,0x01);//00
		write_cmos_sensor(0x3801,0x80);//00
		write_cmos_sensor(0x3802,0x02);//00
		write_cmos_sensor(0x3803,0x8b);//06
		write_cmos_sensor(0x3804,0x10);//12
		write_cmos_sensor(0x3805,0xe1);//61
		write_cmos_sensor(0x3806,0x0b);//0d
		write_cmos_sensor(0x3807,0x12);//97
		write_cmos_sensor(0x3808,0x05);//06
		write_cmos_sensor(0x3809,0x00);//00
		write_cmos_sensor(0x380a,0x02);//04
		write_cmos_sensor(0x380b,0xd0);//80
		write_cmos_sensor(0x380c,0x01);//02
		write_cmos_sensor(0x380d,0xf0);//38
		write_cmos_sensor(0x380e,0x03);//04
		write_cmos_sensor(0x380f,0x80);//0e;96
		write_cmos_sensor(0x3811,0x0f);//
		write_cmos_sensor(0x3813,0x02);
		write_cmos_sensor(0x3814,0x03);
		write_cmos_sensor(0x3815,0x03);
		write_cmos_sensor(0x3820,0x00);
		write_cmos_sensor(0x3821,0x0e);
		write_cmos_sensor(0x3829,0x00);
		write_cmos_sensor(0x382a,0x05);
		write_cmos_sensor(0x382b,0x01);
		write_cmos_sensor(0x3830,0x06);
		write_cmos_sensor(0x3f08,0x20);
		write_cmos_sensor(0x4002,0x01);
		write_cmos_sensor(0x4003,0x02);
		write_cmos_sensor(0x4837,0x14);
		write_cmos_sensor(0x3501,0x49);
		write_cmos_sensor(0x3502,0x20);
		write_cmos_sensor(0x3508,0x08);
		write_cmos_sensor(0x3509,0xff);
		write_cmos_sensor(0x3638,0x00); //;activate 36xx
		write_cmos_sensor(0x301a,0xf0);
		write_cmos_sensor(0x3208,0x10);
		write_cmos_sensor(0x3208,0xa0);
		
		write_cmos_sensor(0x0100,0x01);
			
		mDELAY(30);

}
#if 0
static void fhd_60fps(void)
{
	LOG_INF("E\n");

	/*
	* @@ 0 50 MIPI 4-Lane 1920x1080 10-bit VHbinning2 60fps 800Mbps/lane
	* ;;PCLK=HTS*VTS*fps=0x4b6*0x452*60=1206*1106*60=80M
	* 100 99 1920 1080
	* 100 98 1 1
	* 102 81 0 ffff
	* 102 84 1 ffff
	* 102 3601 1770
	* 102 3f00 452
	*/
	/* ;Sensor Setting  */

	write_cmos_sensor(0x0100, 0x00);
	/*;sl 100 100*/
	mDELAY(100);
	write_cmos_sensor(0x0302, 0x64);

	/*;don't change any PLL VCO in group hold*/
	write_cmos_sensor(0x0305, 0x01);
	write_cmos_sensor(0x030e, 0x01);

	write_cmos_sensor(0x3018, 0x7a);
	write_cmos_sensor(0x3031, 0x0a);
	write_cmos_sensor(0x3603, 0x00);
	write_cmos_sensor(0x3604, 0x00);
	write_cmos_sensor(0x360a, 0x00);
	write_cmos_sensor(0x360b, 0x82);
	write_cmos_sensor(0x360c, 0x1a);
	write_cmos_sensor(0x360d, 0x00);
	write_cmos_sensor(0x3614, 0x75);
	write_cmos_sensor(0x3616, 0x30);
	write_cmos_sensor(0x3631, 0x60);
	write_cmos_sensor(0x3700, 0x30);
	write_cmos_sensor(0x3701, 0x08);
	write_cmos_sensor(0x3702, 0x11);
	write_cmos_sensor(0x3703, 0x20);
	write_cmos_sensor(0x3704, 0x08);
	write_cmos_sensor(0x3705, 0x00);
	write_cmos_sensor(0x3706, 0x9a);
	write_cmos_sensor(0x3708, 0x20);
	write_cmos_sensor(0x3709, 0x3c);
	write_cmos_sensor(0x370a, 0x01);
	write_cmos_sensor(0x370b, 0x6f);
	write_cmos_sensor(0x370c, 0x03);
	write_cmos_sensor(0x370e, 0x20);
	write_cmos_sensor(0x370f, 0x05);
	write_cmos_sensor(0x3710, 0x20);
	write_cmos_sensor(0x3711, 0x20);
	write_cmos_sensor(0x3714, 0x31);
	write_cmos_sensor(0x3719, 0x13);
	write_cmos_sensor(0x371b, 0x03);
	write_cmos_sensor(0x371d, 0x03);
	write_cmos_sensor(0x371e, 0x09);
	write_cmos_sensor(0x371f, 0x17);
	write_cmos_sensor(0x3720, 0x0b);
	write_cmos_sensor(0x3721, 0x18);
	write_cmos_sensor(0x3722, 0x0b);
	write_cmos_sensor(0x3723, 0x18);
	write_cmos_sensor(0x3724, 0x04);
	write_cmos_sensor(0x3725, 0x04);
	write_cmos_sensor(0x3726, 0x02);
	write_cmos_sensor(0x3727, 0x02);
	write_cmos_sensor(0x3728, 0x02);
	write_cmos_sensor(0x3729, 0x02);
	write_cmos_sensor(0x372a, 0x25);
	write_cmos_sensor(0x372b, 0x65);
	write_cmos_sensor(0x372c, 0x55);
	write_cmos_sensor(0x372d, 0x65);
	write_cmos_sensor(0x372e, 0x53);
	write_cmos_sensor(0x372f, 0x33);
	write_cmos_sensor(0x3730, 0x33);
	write_cmos_sensor(0x3731, 0x33);
	write_cmos_sensor(0x3732, 0x03);
	write_cmos_sensor(0x3734, 0x90);
	write_cmos_sensor(0x3739, 0x03);
	write_cmos_sensor(0x373a, 0x20);
	write_cmos_sensor(0x373b, 0x0c);
	write_cmos_sensor(0x373c, 0x1c);
	write_cmos_sensor(0x373e, 0x0b);
	write_cmos_sensor(0x373f, 0x80);

	write_cmos_sensor(0x3800, 0x01);
	write_cmos_sensor(0x3801, 0x80);
	write_cmos_sensor(0x3802, 0x02);
	write_cmos_sensor(0x3803, 0x94);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0xbf);
	write_cmos_sensor(0x3806, 0x0b);
	write_cmos_sensor(0x3807, 0x0f);
	write_cmos_sensor(0x3808, 0x07);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x04);
	write_cmos_sensor(0x380b, 0x38);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0xb6);
	write_cmos_sensor(0x380e, 0x04);
	write_cmos_sensor(0x380f, 0x52);
	write_cmos_sensor(0x3811, 0x17);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3820, 0x01);
	write_cmos_sensor(0x3821, 0x07);
	write_cmos_sensor(0x3829, 0x02);
	write_cmos_sensor(0x382a, 0x03);
	write_cmos_sensor(0x382b, 0x01);
	write_cmos_sensor(0x3830, 0x08);

	write_cmos_sensor(0x3f00, 0x02);
	write_cmos_sensor(0x3f02, 0x00);
	write_cmos_sensor(0x3f04, 0x00);
	write_cmos_sensor(0x3f05, 0x00);
	write_cmos_sensor(0x3f08, 0x20);
	write_cmos_sensor(0x4002, 0x02);
	write_cmos_sensor(0x4003, 0x04);
	write_cmos_sensor(0x4306, 0x00);
	write_cmos_sensor(0x4837, 0x14);

	write_cmos_sensor(0x3501, 0x44);
	write_cmos_sensor(0x3502, 0xe0);
	write_cmos_sensor(0x3508, 0x08);
	write_cmos_sensor(0x3509, 0xff);

	write_cmos_sensor(0x3638, 0x00); /*;activate 36xx*/

	write_cmos_sensor(0x0100, 0x01);
	mDELAY(40);
}
#endif
static void hs_video_setting(void)
{
	LOG_INF("OV16825-120fps\n");
	
	/*	
		@@ MIPI 4-Lane 1920x1080 121fps - reduce HTS then add VTS
		;10-bit Vskip2 Hbinning2 121fps 800Mbps/lane
		;;PCLK=HTS*VTS*fps=0x420*0x4e0*120=1056*1248*120=160M
		100 99 1920 1080
		100 98 1 1
		102 81 0 ffff
		102 84 1 ffff
		102 3601 2ee0
		102 3f00 452
		
		; 1920x1080 4 lane			  setting
		; Mipi: 4 lane
		; width 				 :1920 (0x900)
		; height				 :1080 (0x6c0)
		;FPS=121.4
		;tLine=6.6us
		;HTS = 0x420 = 1056
		;VTS = 0x4e0 = 1248
		;v-blanking=168 (168x6.6=1.1ms)
		;*/
	
		
	
		write_cmos_sensor(0x0100,0x00);
		//;Sensor Setting
		//;group 0
		mDELAY(30);
		write_cmos_sensor(0x3208,0x00);
		write_cmos_sensor(0x301a,0xfb);
		write_cmos_sensor(0x0302,0x64);//800Mbps_lane
		//don'tchange any PLL VCO in group hold
		write_cmos_sensor(0x0305,0x01);
		write_cmos_sensor(0x030e,0x00);
		write_cmos_sensor(0x3018,0x7a);
		write_cmos_sensor(0x3031,0x0a);
		write_cmos_sensor(0x3603,0x05);
		write_cmos_sensor(0x3604,0x02);
		write_cmos_sensor(0x360a,0x02);
		write_cmos_sensor(0x360b,0x02);
		write_cmos_sensor(0x360c,0x12);
		write_cmos_sensor(0x360d,0x04);
		write_cmos_sensor(0x3614,0x77);
		write_cmos_sensor(0x3616,0x31);
		write_cmos_sensor(0x3631,0x40);
		write_cmos_sensor(0x3700,0x60);
		write_cmos_sensor(0x3701,0x10);
		write_cmos_sensor(0x3702,0x22);
		write_cmos_sensor(0x3703,0x40);
		write_cmos_sensor(0x3704,0x10);
		write_cmos_sensor(0x3705,0x01);
		write_cmos_sensor(0x3706,0x04);
		write_cmos_sensor(0x3708,0x40);
		write_cmos_sensor(0x3709,0x78);
		write_cmos_sensor(0x370a,0x02);
		write_cmos_sensor(0x370b,0xb2);
		write_cmos_sensor(0x370c,0x06);
		write_cmos_sensor(0x370e,0x40);
		write_cmos_sensor(0x370f,0x0a);
		write_cmos_sensor(0x3710,0x30);
		write_cmos_sensor(0x3711,0x40);
		write_cmos_sensor(0x3714,0x31);
		write_cmos_sensor(0x3719,0x25);
		write_cmos_sensor(0x371b,0x05);
		write_cmos_sensor(0x371d,0x05);
		write_cmos_sensor(0x371e,0x11);
		write_cmos_sensor(0x371f,0x2d);
		write_cmos_sensor(0x3720,0x15);
		write_cmos_sensor(0x3721,0x30);
		write_cmos_sensor(0x3722,0x15);
		write_cmos_sensor(0x3723,0x30);
		write_cmos_sensor(0x3724,0x08);
		write_cmos_sensor(0x3725,0x08);
		write_cmos_sensor(0x3726,0x04);
		write_cmos_sensor(0x3727,0x04);
		write_cmos_sensor(0x3728,0x04);
		write_cmos_sensor(0x3729,0x04);
		write_cmos_sensor(0x372a,0x29);
		write_cmos_sensor(0x372b,0xc9);
		write_cmos_sensor(0x372c,0xa9);
		write_cmos_sensor(0x372d,0xb9);
		write_cmos_sensor(0x372e,0x95);
		write_cmos_sensor(0x372f,0x55);
		write_cmos_sensor(0x3730,0x55);
		write_cmos_sensor(0x3731,0x55);
		write_cmos_sensor(0x3732,0x05);
		write_cmos_sensor(0x3734,0x10);
		write_cmos_sensor(0x3739,0x05);
		write_cmos_sensor(0x373a,0x40);
		write_cmos_sensor(0x373b,0x18);
		write_cmos_sensor(0x373c,0x38);
		write_cmos_sensor(0x373e,0x15);
		write_cmos_sensor(0x373f,0x80);
		write_cmos_sensor(0x3800,0x01);
		write_cmos_sensor(0x3801,0x80);
		write_cmos_sensor(0x3802,0x02);
		write_cmos_sensor(0x3803,0x94);
		write_cmos_sensor(0x3804,0x10);
		write_cmos_sensor(0x3805,0xbf);
		write_cmos_sensor(0x3806,0x0b);
		write_cmos_sensor(0x3807,0x0f);
		write_cmos_sensor(0x3808,0x07);
		write_cmos_sensor(0x3809,0x80);
		write_cmos_sensor(0x380a,0x04);
		write_cmos_sensor(0x380b,0x38);
		write_cmos_sensor(0x380c,0x04);
		write_cmos_sensor(0x380d,0x20);//b6
		write_cmos_sensor(0x380e,0x04);
		write_cmos_sensor(0x380f,0xe0);//48
		write_cmos_sensor(0x3811,0x17);
		write_cmos_sensor(0x3813,0x02);
		write_cmos_sensor(0x3814,0x03);
		write_cmos_sensor(0x3815,0x01);
		write_cmos_sensor(0x3820,0x00);
		write_cmos_sensor(0x3821,0x07);
		write_cmos_sensor(0x3829,0x00);
		write_cmos_sensor(0x382a,0x03);
		write_cmos_sensor(0x382b,0x01);
		write_cmos_sensor(0x3830,0x08);
		write_cmos_sensor(0x3f08,0x40);
		write_cmos_sensor(0x4002,0x02);
		write_cmos_sensor(0x4003,0x04);
		write_cmos_sensor(0x4837,0x14);
		write_cmos_sensor(0x3501,0x44);
		write_cmos_sensor(0x3502,0xe0);
		write_cmos_sensor(0x3508,0x08);
		write_cmos_sensor(0x3509,0xff);
		write_cmos_sensor(0x3638,0x00);//;activate 36xx
		write_cmos_sensor(0x301a,0xf0);
		write_cmos_sensor(0x3208,0x10);
		write_cmos_sensor(0x3208,0xa0);
		//@@ Sensor scaling for 1920x1080(ratio=1.0)
		//100 99 1920 1080
		write_cmos_sensor(0x3808,0x07);
		write_cmos_sensor(0x3809,0x80);
		write_cmos_sensor(0x380a,0x04);
		write_cmos_sensor(0x380b,0x38);
		write_cmos_sensor(0x3810,0x00);
		write_cmos_sensor(0x3811,0x17);//;11
		write_cmos_sensor(0x3812,0x00);
		write_cmos_sensor(0x3813,0x02);//;04
		write_cmos_sensor(0x4603,0x2f);//;60
		write_cmos_sensor(0x5000,0x7f); 
		
		write_cmos_sensor(0x0100,0x01);
			
		mDELAY(30);
	
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
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor(0x300A) << 16) | (read_cmos_sensor(0x300B)<<8) | read_cmos_sensor(0x300C));
			LOG_INF("read sensor id: 300a=0x%x, 300b=0x%x, 300c=0x%x\n", (read_cmos_sensor(0x300A) << 16),(read_cmos_sensor(0x300B)<<8),read_cmos_sensor(0x300C));
			if (*sensor_id == imgsensor_info.sensor_id) {				
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);	  
				return ERROR_NONE;
			}	
			LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		retry = 2;
	}
	//if (*sensor_id != imgsensor_info.sensor_id) {
	
	if (*sensor_id != 0x16820) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF 
		//*sensor_id = imgsensor_info.sensor_id;
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
	//const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0; 
	kal_uint8 r2a_ic = 0;
	LOG_INF("PLATFORM:MT6595,MIPI 4LANE\n");
	LOG_INF("preview 2304*1728@30fps,800Mbps/lane; video 2304*1728@30fps,800Mbps/lane; capture 16M@24fps,1096Mbps/lane\n");
	
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor(0x300A) << 16) | (read_cmos_sensor(0x300B)<<8) | read_cmos_sensor(0x300C));
			
			if (sensor_id == imgsensor_info.sensor_id) {				
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);	  
				break;
			}	
			LOG_INF("Read sensor id fail, id: 0x%x\n", sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}		 
	if (imgsensor_info.sensor_id != sensor_id){
		LOG_INF("open get sensor id error\n");   
		
		return ERROR_SENSOR_CONNECT_FAIL;
		}
	
	/* initail sequence write in  */
	sensor_init();
	r2a_ic = read_cmos_sensor(0x302A);
	LOG_INF("ov16825 r2a_ic 0x302a = 0x%x\n", r2a_ic);   
	
	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
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
	if (imgsensor.current_fps == 200) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps); 
	
	
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
	normal_video_setting(imgsensor.current_fps);
	
	
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
	//imgsensor.current_fps = 1200;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	
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
	//preview_setting();
	
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

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
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
	
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
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
		default:			
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		
			
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}
	
	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
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


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate) 
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
			//set_dummy();			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;			
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:		
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;	
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;	
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();	
			break;		
		default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();	
			LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}	
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate) 
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
		// 0x5040[8]: 1 enable,  0 disable
		// 0x5040[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x5040, 0x80);
	} else {
		// 0x5040[8]: 1 enable,  0 disable
		// 0x5040[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x5040, 0x00);
	}	 
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16=(UINT16 *) feature_para;
	UINT16 *feature_data_16=(UINT16 *) feature_para;
	UINT32 *feature_return_para_32=(UINT32 *) feature_para;
	UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;
    //unsigned long long *feature_return_para=(unsigned long long *) feature_para;
	
	SENSOR_WINSIZE_INFO_STRUCT *wininfo;	
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
 
	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:	 
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len=4;
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
			write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
			break; 
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32); 
			break; 
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing			 
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len=4;							 
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
            LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
            wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		
			switch (*feature_data_32) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;	  
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
			}
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			break;
		default:
			break;
	}
  
	return ERROR_NONE;
}	/*	feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

/*UINT32 OV16825_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)*/
UINT32 OV16825MIPISensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)

{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	OV16825_MIPI_RAW_SensorInit	*/
