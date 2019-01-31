/*
 * Copyright (C) 2016 MediaTek Inc.
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

/************************************************************************
 *
 * Filename:
 * ---------
 *     IMX338mipi_Sensor.c
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
#define PFX "IMX338_camera_sensor"
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

#include "imx338mipi_Sensor.h"


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


#define BYTE               unsigned char

/* static BOOL read_spc_flag = FALSE; */

/*support ZHDR*/
/* #define IMX338_ZHDR */

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static BYTE imx338_SPC_data[352] = { 0 };


static struct imgsensor_info_struct imgsensor_info = {
	/* record sensor id defined in Kd_imgsensor.h */
	.sensor_id = IMX338_SENSOR_ID,

	/* checksum value for Camera Auto Test */
	.checksum_value = 0x6c259b92,

	/*data rate 1099.20 Mbps/lane */
	.pre = {
		.pclk = 531000000,/* record different mode's pclk */
		.linelength = 6024,/* record different mode's linelength */
		.framelength = 2896,/* record different mode's framelength */
		.startx = 0,/* record different mode's startx of grabwindow */
		.starty = 0,/* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2672,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 2008,

		/* following for settle count by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,

		/* following for GetDefaultFramerateByScenario()    */
		.max_framerate = 300,
		.mipi_pixel_rate = 313000000,
	},

	/*data rate 1499.20 Mbps/lane */
	.cap = {
		.pclk = 600000000,
		.linelength = 6024,
		.framelength = 4150,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5344,
		.grabwindow_height = 4016,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 240,
		.mipi_pixel_rate = 600000000,
	},

	/*data rate 1499.20 Mbps/lane */
	.cap1 = {
		.pclk = 600000000,
		.linelength = 6024,
		.framelength = 4150,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5344,
		.grabwindow_height = 4016,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 240,
		.mipi_pixel_rate = 600000000,
	},

	/*data rate 1499.20 Mbps/lane */
	.normal_video = {
		.pclk = 600000000,
		.linelength = 6024,
		.framelength = 3304,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5344,
		.grabwindow_height = 3008,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 300,
		.mipi_pixel_rate = 600000000,
	},

	/*data rate 600 Mbps/lane */
	.hs_video = {
		.pclk = 595000000,
		.linelength = 6024,
		.framelength = 828,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 736,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 1200,
		.mipi_pixel_rate = 241000000,
	},

	/*data rate 792 Mbps/lane */
	.slim_video = {
		.pclk = 531000000,
		.linelength = 6024,
		.framelength = 1470,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 736,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 596,
		.mipi_pixel_rate = 317000000,
	},

	/*data rate 1099.20 Mbps/lane */
	.custom1 = {

		.pclk = 531000000,/* record different mode's pclk */
		.linelength = 6024,/* record different mode's linelength */
		.framelength = 2896,/* record different mode's framelength */
		.startx = 0,/* record different mode's startx of grabwindow */
		.starty = 0,/* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2672,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 2008,

		/* following for settle count by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,

		/* following for GetDefaultFramerateByScenario()    */
		.max_framerate = 300,
		.mipi_pixel_rate = 313000000,
	},

	/*data rate 1099.20 Mbps/lane */
	.custom2 = {
		.pclk = 531000000,/* record different mode's pclk */
		.linelength = 6024,/* record different mode's linelength */
		.framelength = 2896,/* record different mode's framelength */
		.startx = 0,/* record different mode's startx of grabwindow */
		.starty = 0,/* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2672,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 2008,

		/* following for settle count by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,

		/* following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 313000000,
	},

	/*data rate 1099.20 Mbps/lane */
	.custom3 = {
		.pclk = 531000000,/* record different mode's pclk */
		.linelength = 6024,/* record different mode's linelength */
		.framelength = 2896,/* record different mode's framelength */
		.startx = 0,/* record different mode's startx of grabwindow */
		.starty = 0,/* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2672,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 2008,

		/* following for settle count by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,

		/* following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 313000000,
	},

	/*data rate 1099.20 Mbps/lane */
	.custom4 = {
		.pclk = 531000000,/* record different mode's pclk */
		.linelength = 6024,/* record different mode's linelength */
		.framelength = 2896,/* record different mode's framelength */
		.startx = 0,/* record different mode's startx of grabwindow */
		.starty = 0,/* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2672,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 2008,

		/* following for settle count by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,

		/* following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 313000000,
	},

	/*data rate 1099.20 Mbps/lane */
	.custom5 = {
		.pclk = 531000000,/* record different mode's pclk */
		.linelength = 6024,/* record different mode's linelength */
		.framelength = 2896,/* record different mode's framelength */
		.startx = 0,/* record different mode's startx of grabwindow */
		.starty = 0,/* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2672,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 2008,

		/* following for settle count by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,

		/* following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 313000000,
	},

	.margin = 10,		/* sensor framelength & shutter margin */
	.min_shutter = 1,	/* min shutter */

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
	.sensor_mode_num = 10,	/* support sensor mode num */

	.cap_delay_frame = 1,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 1,	/* enter video delay frame num */
	.hs_video_delay_frame = 3,/* enter high speed video  delay frame num */
	.slim_video_delay_frame = 3,/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_4MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,

	/* sensor output first pixel color */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */

	/* record sensor support all write id addr,
	 * only supprt 4must end with 0xff
	 */
	.i2c_addr_table = {0x34, 0x20, 0xff},
	.i2c_speed = 400,	/* i2c read/write speed */
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

	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.hdr_mode = 0,/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x6c,/* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
{5344, 4016, 0, 0, 5344, 4016, 2672, 2008,
	0000, 0000, 2672, 2008, 0, 0, 2672, 2008},	/* Preview */

{5344, 4016, 0, 0, 5344, 4016, 5344, 4016,
	0000, 0000, 5344, 4016, 0, 0, 5344, 4016},	/* capture */

{5344, 4016, 0, 504, 5344, 3008, 5344, 3008,
	0000, 0000, 5344, 3008, 0, 0, 5344, 3008},	/* video */

{5344, 4016, 0, 536, 5344, 2944, 1336, 736,
	20, 0000, 1296, 736, 0, 0, 1296, 736},

{5344, 4016, 0, 536, 5344, 2944, 1336, 736,
	20, 0000, 1296, 736, 0, 0, 1296, 736},

{5344, 4016, 0, 0, 5344, 4016, 2672, 2008,
	0000, 0000, 2672, 2008, 0, 0, 2672, 2008},

{5344, 4016, 0, 0, 5344, 4016, 2672, 2008,
	0000, 0000, 2672, 2008, 0, 0, 2672, 2008},

{5344, 4016, 0, 0, 5344, 4016, 2672, 2008,
	0000, 0000, 2672, 2008, 0, 0, 2672, 2008},

{5344, 4016, 0, 0, 5344, 4016, 2672, 2008,
	0000, 0000, 2672, 2008, 0, 0, 2672, 2008},

{5344, 4016, 0, 0, 5344, 4016, 2672, 2008,
	0000, 0000, 2672, 2008, 0, 0, 2672, 2008}
};

 /*VC1 for HDR(DT=0X35) , VC2 for PDAF(DT=0X36), unit : 10bit */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Preview mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0A70, 0x07D8, 0x00, 0x35, 0x0280, 0x0001,
	 0x00, 0x36, 0x0C48, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x14E0, 0x0FB0, 0x00, 0x35, 0x0280, 0x0001,
	 0x00, 0x36, 0x1a18, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x14E0, 0x0FB0, 0x00, 0x35, 0x0280, 0x0001,
	 0x02, 0x00, 0x0000, 0x0000, 0x03, 0x00, 0x0000, 0x0000}
};

struct SENSOR_ATR_INFO {
	MUINT16 DarkLimit_H;
	MUINT16 DarkLimit_L;
	MUINT16 OverExp_Min_H;
	MUINT16 OverExp_Min_L;
	MUINT16 OverExp_Max_H;
	MUINT16 OverExp_Max_L;
};
#if 0
static SENSOR_ATR_INFO sensorATR_Info[4] = {	/* Strength Range Min */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	/* Strength Range Std */
	{0x00, 0x32, 0x00, 0x3c, 0x03, 0xff},
	/* Strength Range Max */
	{0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xff},
	/* Strength Range Custom */
	{0x3F, 0xFF, 0x00, 0x0, 0x3F, 0xFF}
};
#endif

#define IMX338MIPI_MaxGainIndex (115)
kal_uint16 IMX338MIPI_sensorGainMapping[IMX338MIPI_MaxGainIndex][2] = {
	{64, 0},
	{65, 8},
	{66, 16},
	{67, 25},
	{68, 30},
	{69, 37},
	{70, 45},
	{71, 51},
	{72, 57},
	{73, 63},
	{74, 67},
	{75, 75},
	{76, 81},
	{77, 85},
	{78, 92},
	{79, 96},
	{80, 103},
	{81, 107},
	{82, 112},
	{83, 118},
	{84, 122},
	{86, 133},
	{88, 140},
	{89, 144},
	{90, 148},
	{93, 159},
	{96, 171},
	{97, 175},
	{99, 182},
	{101, 188},
	{102, 192},
	{104, 197},
	{106, 202},
	{107, 206},
	{109, 211},
	{112, 220},
	{113, 222},
	{115, 228},
	{118, 235},
	{120, 239},
	{125, 250},
	{126, 252},
	{128, 256},
	{129, 258},
	{130, 260},
	{132, 264},
	{133, 266},
	{135, 269},
	{136, 271},
	{138, 274},
	{139, 276},
	{141, 279},
	{142, 282},
	{144, 285},
	{145, 286},
	{147, 290},
	{149, 292},
	{150, 294},
	{155, 300},
	{157, 303},
	{158, 305},
	{161, 309},
	{163, 311},
	{170, 319},
	{172, 322},
	{174, 324},
	{176, 326},
	{179, 329},
	{181, 331},
	{185, 335},
	{189, 339},
	{193, 342},
	{195, 344},
	{196, 345},
	{200, 348},
	{202, 350},
	{205, 352},
	{207, 354},
	{210, 356},
	{211, 357},
	{214, 359},
	{217, 361},
	{218, 362},
	{221, 364},
	{224, 366},
	{231, 370},
	{237, 374},
	{246, 379},
	{250, 381},
	{252, 382},
	{256, 384},
	{260, 386},
	{262, 387},
	{273, 392},
	{275, 393},
	{280, 395},
	{290, 399},
	{306, 405},
	{312, 407},
	{321, 410},
	{331, 413},
	{345, 417},
	{352, 419},
	{360, 421},
	{364, 422},
	{372, 424},
	{386, 427},
	{400, 430},
	{410, 432},
	{420, 434},
	{431, 436},
	{437, 437},
	{449, 439},
	{468, 442},
	{512, 448},
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

#if 1
#define ATR_DARK_THRESHOLD 51
static kal_uint32 imx338_ATR(UINT16 DarkLimit, UINT16 OverExp)
{
	/*write_cmos_sensor(0xAA16, 0x01);*/
	/*write_cmos_sensor(0xAA17, 0x00);*/
	/*write_cmos_sensor(0xAA18, 0x00);*/
	/*write_cmos_sensor(0xAA19, 0x3C);*/
	/*write_cmos_sensor(0xAA1A, 0x10);*/
	/*write_cmos_sensor(0xAA1B, 0x00);*/
	if (DarkLimit > ATR_DARK_THRESHOLD) {
		write_cmos_sensor(0xAA16, 0x00);
		write_cmos_sensor(0xAA17, 0x64);
		write_cmos_sensor(0xAA18, 0x08);
		write_cmos_sensor(0xAA19, 0x00);
		write_cmos_sensor(0xAA1A, 0x10);
		write_cmos_sensor(0xAA1B, 0x00);
	} else {
		write_cmos_sensor(0xAA16, 0x00);
		write_cmos_sensor(0xAA17, 0x96);
		write_cmos_sensor(0xAA18, 0x00);
		write_cmos_sensor(0xAA19, 0xc8);
		write_cmos_sensor(0xAA1A, 0x01);
		write_cmos_sensor(0xAA1B, 0x00);
	}
	/*pr_debug("bk_ imx338_ATR 0x%x-%x, 0x%x-%x, 0x%x-%x DarkLimit %d",
	 *	read_cmos_sensor(0xAA16), read_cmos_sensor(0xAA17),
	 *	read_cmos_sensor(0xAA18), read_cmos_sensor(0xAA19),
	 *	read_cmos_sensor(0xAA1A), read_cmos_sensor(0xAA1B),DarkLimit);
	 */

	return ERROR_NONE;
}
#endif

static MUINT32 cur_startpos;
static MUINT32 cur_size;
static void imx338_set_pd_focus_area(MUINT32 startpos, MUINT32 size)
{
	UINT16 start_x_pos, start_y_pos, end_x_pos, end_y_pos;
	UINT16 focus_width, focus_height;

	if ((cur_startpos == startpos) && (cur_size == size)) {
		pr_debug("Not to need update focus area!\n");
		return;
	}
	cur_startpos = startpos;
	cur_size = size;

	start_x_pos = (startpos >> 16) & 0xFFFF;
	start_y_pos = startpos & 0xFFFF;
	focus_width = (size >> 16) & 0xFFFF;
	focus_height = size & 0xFFFF;

	end_x_pos = start_x_pos + focus_width;
	end_y_pos = start_y_pos + focus_height;

	if (imgsensor.pdaf_mode == 1) {
		pr_debug("GC pre PDAF\n");
		/*PDAF*/
		/*PD_CAL_ENALBE */
		write_cmos_sensor(0x3121, 0x01);
		/*AREA MODE */
		write_cmos_sensor(0x31B0, 0x02);	/* 8x6 output */
		write_cmos_sensor(0x31B4, 0x01);	/* 8x6 output */
		/*PD_OUT_EN=1 */
		write_cmos_sensor(0x3123, 0x01);

		/*Fixed area mode */

		write_cmos_sensor(0x3158, (start_x_pos >> 8) & 0xFF);
		write_cmos_sensor(0x3159, start_x_pos & 0xFF);	/* X start */
		write_cmos_sensor(0x315a, (start_y_pos >> 8) & 0xFF);
		write_cmos_sensor(0x315b, start_y_pos & 0xFF);	/* Y start */
		write_cmos_sensor(0x315c, (end_x_pos >> 8) & 0xFF);
		write_cmos_sensor(0x315d, end_x_pos & 0xFF);	/* X end */
		write_cmos_sensor(0x315e, (end_y_pos >> 8) & 0xFF);
		write_cmos_sensor(0x315f, end_y_pos & 0xFF);	/* Y end */


	}


	pr_debug(
	    "start_x:%d, start_y:%d, width:%d, height:%d, end_x:%d, end_y:%d\n",
	    start_x_pos,
	    start_y_pos,
	    focus_width,
	    focus_height,
	    end_x_pos,
	    end_y_pos);


}


static void imx338_get_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor(regDa[idx]);
		/* pr_debug("%x %x", regDa[idx], regDa[idx+1]); */
	}
}

static void imx338_set_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor(regDa[idx], regDa[idx + 1]);
		/* pr_debug("%x %x", regDa[idx], regDa[idx+1]); */
	}
}




static void imx338_apply_SPC(void)
{
	unsigned int start_reg = 0x7c00;
	char puSendCmd[355];
	kal_uint32 tosend;


	pr_debug("E");

	read_imx338_SPC(imx338_SPC_data);

	tosend = 0;
	puSendCmd[tosend++] = (char)(start_reg >> 8);
	puSendCmd[tosend++] = (char)(start_reg & 0xFF);
	memcpy((void *)&puSendCmd[tosend], imx338_SPC_data, 352);
	tosend += 352;
	iBurstWriteReg_multi(
	    puSendCmd,
	    tosend,
	    imgsensor.i2c_write_id,
	    tosend,
	    imgsensor_info.i2c_speed);

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
} /* set_dummy  */

static kal_uint32 return_lot_id_from_otp(void)
{
	kal_uint16 val = 0;
	int i = 0;

	if (write_cmos_sensor(0x0a02, 0x1f) < 0) {
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
	if (i == 5) {
		pr_debug("read otp fail Err!\n");
		return 0;
	}
	/* pr_debug("0x0A38 0x%x 0x0A39 0x%x\n",
	 *read_cmos_sensor(0x0A38)<<4,
	 *read_cmos_sensor(0x0A39)>>4);
	 */
	return((read_cmos_sensor(0x0A38) << 4) | read_cmos_sensor(0x0A39) >> 4);
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
		imgsensor.dummy_line =
		    imgsensor.frame_length - imgsensor.min_frame_length;

	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*    set_max_framerate  */



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

	/* pr_debug("Enter! shutter =%d, framelength =%d\n",
	 *  shutter,
	 * imgsensor.frame_length);
	 */

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
	    ? imgsensor_info.min_shutter : shutter;

	/* long expsoure */
	if (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) {

		for (l_shift = 1; l_shift < MAX_CIT_LSHIFT; l_shift++) {
			if ((shutter >> l_shift)
		    < (imgsensor_info.max_frame_length - imgsensor_info.margin))

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
		/* pr_debug(
		 *  "0x3028 0x%x l_shift %d l_shift&0x3 %d\n",
		 *  read_cmos_sensor(0x3028),
		 *  l_shift,
		 *  l_shift&0x7);
		 */

		write_cmos_sensor(0x3028,
		    read_cmos_sensor(0x3028) | (l_shift & 0x7));

		/* pr_debug("0x3028 0x%x\n", read_cmos_sensor(0x3028)); */

	} else {
		write_cmos_sensor(0x3028, read_cmos_sensor(0x3028) & 0xf8);
	}

	shutter =
	   (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	  ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps =
	imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

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



static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 iI;

	pr_debug("[IMX338MIPI]enter IMX338MIPIGain2Reg function\n");
	for (iI = 0; iI < IMX338MIPI_MaxGainIndex; iI++) {
		if (gain <= IMX338MIPI_sensorGainMapping[iI][0])
			return IMX338MIPI_sensorGainMapping[iI][1];


	}
	pr_debug("exit IMX338MIPIGain2Reg function\n");
	return IMX338MIPI_sensorGainMapping[iI - 1][1];
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

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < BASEGAIN || gain > 8 * BASEGAIN) {
		pr_debug("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 8 * BASEGAIN)
			gain = 8 * BASEGAIN;
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
	/* Global analog Gain for Short expo */
	write_cmos_sensor(0x0216, (reg_gain >> 8) & 0xFF);
	write_cmos_sensor(0x0217, reg_gain & 0xFF);
	write_cmos_sensor(0x0104, 0x00);


	return gain;
}				/*    set_gain  */

/************************************************************************
 * FUNCTION
 *    set_dual_gain
 *
 * DESCRIPTION
 *    This function is to set dual gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor dual gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static kal_uint16 set_dual_gain(kal_uint16 gain1, kal_uint16 gain2)
{
	kal_uint16 reg_gain1, reg_gain2;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */


	if (gain1 < BASEGAIN || gain1 > 8 * BASEGAIN) {
		pr_debug("Error gain1 setting");

		if (gain1 < BASEGAIN)
			gain1 = BASEGAIN;
		else if (gain1 > 8 * BASEGAIN)
			gain1 = 8 * BASEGAIN;
	}

	if (gain2 < BASEGAIN || gain2 > 8 * BASEGAIN) {
		pr_debug("Error gain2 setting");

		if (gain2 < BASEGAIN)
			gain2 = BASEGAIN;
		else if (gain2 > 8 * BASEGAIN)
			gain2 = 8 * BASEGAIN;
	}

	reg_gain1 = gain2reg(gain1);
	reg_gain2 = gain2reg(gain2);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain1;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain1 = %d, reg_gain1 = 0x%x, gain2 = %d, reg_gain2 = 0x%x\n ",
		gain1, reg_gain1, gain2, reg_gain2);

	write_cmos_sensor(0x0104, 0x01);
	/* Global analog Gain for Long expo */
	write_cmos_sensor(0x0204, (reg_gain1 >> 8) & 0xFF);
	write_cmos_sensor(0x0205, reg_gain1 & 0xFF);
	/* Global analog Gain for Short expo */
	write_cmos_sensor(0x0216, (reg_gain2 >> 8) & 0xFF);
	write_cmos_sensor(0x0217, reg_gain2 & 0xFF);
	write_cmos_sensor(0x0104, 0x00);

	return gain1;

}				/*    set_dual_gain  */

static void hdr_write_shutter(kal_uint16 le, kal_uint16 se, kal_uint16 lv)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 ratio;

	pr_debug("le:0x%x, se:0x%x\n", le, se);
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
	if (imgsensor.autoflicker_en) {
		realtime_fps =
	   imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341,
				imgsensor.frame_length & 0xFF);

			write_cmos_sensor(0x0104, 0x00);
		}
	} else {
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
	}
	write_cmos_sensor(0x0104, 0x01);

	/* Long exposure */
	write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor(0x0203, le & 0xFF);
	/* Short exposure */
	write_cmos_sensor(0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor(0x0225, se & 0xFF);
	write_cmos_sensor(0x0104, 0x00);

	/* Ratio */
	if (se == 0)
		ratio = 2;
	else {
		ratio = (le + (se >> 1)) / se;
		if (ratio > 16)
			ratio = 2;
	}

	pr_debug("le:%d, se:%d, ratio:%d\n", le, se, ratio);
	write_cmos_sensor(0x0222, ratio);

	imx338_ATR(lv, lv);

}

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	pr_debug("image_mirror = %d\n", image_mirror);
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0101, 0x00);
		write_cmos_sensor(0x3A27, 0x00);
		write_cmos_sensor(0x3A28, 0x00);
		write_cmos_sensor(0x3A29, 0x01);
		write_cmos_sensor(0x3A2A, 0x00);
		write_cmos_sensor(0x3A2B, 0x00);
		write_cmos_sensor(0x3A2C, 0x00);
		write_cmos_sensor(0x3A2D, 0x01);
		write_cmos_sensor(0x3A2E, 0x01);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0101, 0x01);
		write_cmos_sensor(0x3A27, 0x01);
		write_cmos_sensor(0x3A28, 0x01);
		write_cmos_sensor(0x3A29, 0x00);
		write_cmos_sensor(0x3A2A, 0x00);
		write_cmos_sensor(0x3A2B, 0x01);
		write_cmos_sensor(0x3A2C, 0x00);
		write_cmos_sensor(0x3A2D, 0x00);
		write_cmos_sensor(0x3A2E, 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0101, 0x02);
		write_cmos_sensor(0x3A27, 0x10);
		write_cmos_sensor(0x3A28, 0x10);
		write_cmos_sensor(0x3A29, 0x01);
		write_cmos_sensor(0x3A2A, 0x01);
		write_cmos_sensor(0x3A2B, 0x00);
		write_cmos_sensor(0x3A2C, 0x01);
		write_cmos_sensor(0x3A2D, 0x01);
		write_cmos_sensor(0x3A2E, 0x00);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0101, 0x03);
		write_cmos_sensor(0x3A27, 0x11);
		write_cmos_sensor(0x3A28, 0x11);
		write_cmos_sensor(0x3A29, 0x00);
		write_cmos_sensor(0x3A2A, 0x01);
		write_cmos_sensor(0x3A2B, 0x01);
		write_cmos_sensor(0x3A2C, 0x01);
		write_cmos_sensor(0x3A2D, 0x00);
		write_cmos_sensor(0x3A2E, 0x00);
		break;
	default:
		pr_debug("Error image_mirror setting\n");
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
}				/*    night_mode    */


#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3

#endif
static kal_uint16 imx338_table_write_cmos_sensor(
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
		    len == IDX  ||
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


kal_uint16 addr_data_pair_imx338_zvhdr_on[] = {
	0x30b1, 0x00,
	0x30c6, 0x00,
	0x30b2, 0x00,
	0x30b3, 0x00,
	0x30c7, 0x00,
	0x30b4, 0x01,
	0x30b5, 0x01,
	0x30b6, 0x01,
	0x30b7, 0x01,
	0x30b8, 0x01,
	0x30b9, 0x01,
	0x30ba, 0x01,
	0x30bb, 0x01,
	0x30bc, 0x01,
};

kal_uint16 addr_data_pair_imx338_zvhdr_off[] = {
	0x30b4, 0x00,
	0x30b5, 0x00,
	0x30b6, 0x00,
	0x30b7, 0x00,
	0x30b8, 0x00,
	0x30b9, 0x00,
	0x30ba, 0x00,
	0x30bb, 0x00,
	0x30bc, 0x00,
};


static kal_uint16 zvhdr_setting(void)
{

	pr_debug("zhdr(mode:%d)\n", imgsensor.hdr_mode);

	if (imgsensor.hdr_mode == 9) {
		imx338_table_write_cmos_sensor(addr_data_pair_imx338_zvhdr_on,
		   sizeof(addr_data_pair_imx338_zvhdr_on) / sizeof(kal_uint16));
	} else {
		imx338_table_write_cmos_sensor(addr_data_pair_imx338_zvhdr_off,
		  sizeof(addr_data_pair_imx338_zvhdr_off) / sizeof(kal_uint16));
	}
	return 0;

}

kal_uint16 addr_data_pair_init_imx338[] = {

	0x0136, 0x18,
	0x0137, 0x00,

	0x0101, 0x03,
	0x4532, 0x05,
	0x4533, 0x01,
	0x4771, 0x0B,
	0x4800, 0x0E,
	0x4890, 0x01,
	0x4D1E, 0x01,
	0x4D1F, 0xFF,
	0x4FA0, 0x00,
	0x4FA1, 0x00,
	0x4FA2, 0x00,
	0x4FA3, 0x83,
	0x6153, 0x01,
	0x6156, 0x01,
	0x69BB, 0x01,
	0x69BC, 0x05,
	0x69BD, 0x05,
	0x69C1, 0x00,
	0x69C4, 0x01,
	0x69C6, 0x01,
	0x7300, 0x00,
	0xB100, 0x63,
	0xB101, 0x63,
	0xB102, 0x63,
	0xB103, 0x63,
	0xB104, 0x63,
	0xB105, 0x63,
	0xB106, 0x63,
	0xB107, 0x63,
	0xD000, 0xDA,
	0xD001, 0xDA,
	0xD002, 0xAF,
	0xD003, 0xE1,
	0xD004, 0x55,
	0xD005, 0x34,
	0xD006, 0x21,
	0xD007, 0x00,
	0xD008, 0x1C,
	0xD009, 0x80,
	0xD00A, 0xFE,
	0xD00B, 0xC5,
	0xD00C, 0x55,
	0xD00D, 0xDC,
	0xD00E, 0xB6,
	0xD00F, 0x00,
	0xD010, 0x31,
	0xD011, 0x02,
	0xD012, 0x4A,
	0xD013, 0x0E,
	0xD014, 0x55,
	0xD015, 0xF0,
	0xD016, 0x1B,
	0xD017, 0x00,
	0xD018, 0xFA,
	0xD019, 0x2C,
	0xD01A, 0xF1,
	0xD01B, 0x7E,
	0xD01C, 0x55,
	0xD01D, 0x1C,
	0xD01E, 0xD8,
	0xD01F, 0x00,
	0xD020, 0x76,
	0xD021, 0xC1,
	0xD022, 0xBF,
	0xD044, 0x40,
	0xD045, 0xBA,
	0xD046, 0x70,
	0xD047, 0x47,
	0xD048, 0xC0,
	0xD049, 0xBA,
	0xD04A, 0x70,
	0xD04B, 0x47,
	0xD04C, 0x82,
	0xD04D, 0xF6,
	0xD04E, 0xDA,
	0xD04F, 0xFA,
	0xD050, 0x00,
	0xD051, 0xF0,
	0xD052, 0x02,
	0xD053, 0xF8,
	0xD054, 0x81,
	0xD055, 0xF6,
	0xD056, 0xCE,
	0xD057, 0xFD,
	0xD058, 0x10,
	0xD059, 0xB5,
	0xD05A, 0x0D,
	0xD05B, 0x48,
	0xD05C, 0x40,
	0xD05D, 0x7A,
	0xD05E, 0x01,
	0xD05F, 0x28,
	0xD060, 0x15,
	0xD061, 0xD1,
	0xD062, 0x0C,
	0xD063, 0x49,
	0xD064, 0x0C,
	0xD065, 0x46,
	0xD066, 0x40,
	0xD067, 0x3C,
	0xD068, 0x48,
	0xD069, 0x8A,
	0xD06A, 0x62,
	0xD06B, 0x8A,
	0xD06C, 0x80,
	0xD06D, 0x1A,
	0xD06E, 0x8A,
	0xD06F, 0x89,
	0xD070, 0x00,
	0xD071, 0xB2,
	0xD072, 0x10,
	0xD073, 0x18,
	0xD074, 0x0A,
	0xD075, 0x46,
	0xD076, 0x20,
	0xD077, 0x32,
	0xD078, 0x12,
	0xD079, 0x88,
	0xD07A, 0x90,
	0xD07B, 0x42,
	0xD07C, 0x00,
	0xD07D, 0xDA,
	0xD07E, 0x10,
	0xD07F, 0x46,
	0xD080, 0x80,
	0xD081, 0xB2,
	0xD082, 0x88,
	0xD083, 0x81,
	0xD084, 0x84,
	0xD085, 0xF6,
	0xD086, 0x06,
	0xD087, 0xF8,
	0xD088, 0xE0,
	0xD089, 0x67,
	0xD08A, 0x85,
	0xD08B, 0xF6,
	0xD08C, 0x4B,
	0xD08D, 0xFC,
	0xD08E, 0x10,
	0xD08F, 0xBD,
	0xD090, 0x00,
	0xD091, 0x18,
	0xD092, 0x1E,
	0xD093, 0x78,
	0xD094, 0x00,
	0xD095, 0x18,
	0xD096, 0x17,
	0xD097, 0x98,
	0x5869, 0x01,

	0x3004,	0x07,
	0x3019,	0x00,
	0x3198,	0x0F,
	0x31A0,	0x04,
	0x31A1,	0x03,
	0x31A2,	0x02,
	0x31A3,	0x01,
	0x31A8,	0x18,
	0x6819,	0x00,
	0x6873,	0x10,
	0x68A9,	0x00,
	0x68C5,	0x00,
	0x68DF,	0x00,
	0x69CB,	0x01,
	0x6B42,	0x40,
	0x6B45,	0x8C,
	0x6B47,	0x4B,
	0x6B49,	0x8C,
	0x6B4B,	0x4B,
	0x6B4D,	0x8C,
	0x6B4F,	0x4B,
	0x822C,	0x01,
	0x8239,	0x01,
	0x8858,	0x00,
	0x9009,	0x03,
	0x9098,	0x1A,
	0x9099,	0x04,
	0x909A,	0x20,
	0x909B,	0x20,
	0x909C,	0x13,
	0x909D,	0x13,
	0x9236,	0x04,
	0x9257,	0x96,
	0x9258,	0x00,
	0x933A,	0x02,
	0x933B,	0x02,
	0x933D,	0x05,
	0x933E,	0x05,
	0x933F,	0x05,
	0x934B,	0x1B,
	0x934C,	0x0A,
	0x9356,	0x8C,
	0x9357,	0x50,
	0x9358,	0x1B,
	0x9359,	0x8C,
	0x935A,	0x1B,
	0x935B,	0x0A,
	0x9360,	0x1B,
	0x9361,	0x0A,
	0x9362,	0x8C,
	0x9363,	0x50,
	0x9364,	0x1B,
	0x9365,	0x8C,
	0x9366,	0x1B,
	0x9367,	0x0A,
	0x940D,	0x07,
	0x940E,	0x07,
	0x9414,	0x06,
	0x942B,	0x09,
	0x942C,	0x09,
	0x942D,	0x09,
	0x942E,	0x09,
	0x942F,	0x09,
	0x9430,	0x09,
	0x9431,	0x09,
	0x9432,	0x09,
	0x9433,	0x09,
	0x9434,	0x09,
	0x9435,	0x09,
	0x9436,	0x09,
	0x9437,	0x09,
	0x9438,	0x0B,
	0x9439,	0x0B,
	0x943B,	0x09,
	0x943D,	0x09,
	0x943F,	0x09,
	0x9441,	0x09,
	0x9443,	0x09,
	0x9445,	0x09,
	0x9447,	0x09,
	0x9449,	0x09,
	0x944B,	0x09,
	0x944D,	0x09,
	0x944F,	0x09,
	0x9451,	0x09,
	0x9452,	0x0A,
	0x9454,	0x09,
	0x9456,	0x09,
	0x945B,	0x07,
	0x945C,	0x0B,
	0x945D,	0x07,
	0x945E,	0x0B,
	0x9463,	0x09,
	0x9464,	0x09,
	0x9465,	0x09,
	0x9466,	0x09,
	0x947C,	0x01,
	0x947D,	0x01,
	0x9480,	0x01,
	0x9481,	0x01,
	0x9503,	0x07,
	0x9504,	0x07,
	0x9505,	0x07,
	0x9506,	0x00,
	0x9507,	0x00,
	0x9508,	0x00,
	0x9526,	0x18,
	0x9527,	0x18,
	0x9528,	0x18,
	0x9619,	0xA0,
	0x961B,	0xA0,
	0x961D,	0xA0,
	0x961F,	0x20,
	0x9621,	0x20,
	0x9623,	0x20,
	0x9625,	0xA0,
	0x9627,	0xA0,
	0x9629,	0xA0,
	0x962B,	0x20,
	0x962D,	0x20,
	0x962F,	0x20,
	0x9719,	0xA0,
	0x971B,	0xA0,
	0x971D,	0xA0,
	0x971F,	0x20,
	0x9721,	0x20,
	0x9723,	0x20,
	0x9725,	0xA0,
	0x9727,	0xA0,
	0x9729,	0xA0,
	0x972B,	0x20,
	0x972D,	0x20,
	0x972F,	0x20,
	0x9901,	0x35,
	0x9903,	0x23,
	0x9905,	0x23,
	0x9906,	0x00,
	0x9907,	0x31,
	0x9908,	0x00,
	0x9909,	0x1B,
	0x990A,	0x00,
	0x990B,	0x15,
	0x990D,	0x3F,
	0x990F,	0x3F,
	0x9911,	0x3F,
	0x9913,	0x64,
	0x9915,	0x64,
	0x9917,	0x64,
	0x9919,	0x50,
	0x991B,	0x60,
	0x991D,	0x65,
	0x991F,	0x01,
	0x9921,	0x01,
	0x9923,	0x01,
	0x9925,	0x23,
	0x9927,	0x23,
	0x9929,	0x23,
	0x992B,	0x2F,
	0x992D,	0x1A,
	0x992F,	0x14,
	0x9931,	0x3F,
	0x9933,	0x3F,
	0x9935,	0x3F,
	0x9937,	0x6B,
	0x9939,	0x7C,
	0x993B,	0x81,
	0x9943,	0x0F,
	0x9945,	0x0F,
	0x9947,	0x0F,
	0x9949,	0x0F,
	0x994B,	0x0F,
	0x994D,	0x0F,
	0x994F,	0x42,
	0x9951,	0x0F,
	0x9953,	0x0B,
	0x9955,	0x5A,
	0x9957,	0x13,
	0x9959,	0x0C,
	0x995A,	0x00,
	0x995B,	0x00,
	0x995C,	0x00,
	0x996B,	0x00,
	0x996D,	0x10,
	0x996F,	0x10,
	0x9971,	0xC8,
	0x9973,	0x32,
	0x9975,	0x04,
	0x99A4,	0x2F,
	0x99A5,	0x2F,
	0x99A6,	0x2F,
	0x99A7,	0x0A,
	0x99A8,	0x0A,
	0x99A9,	0x0A,
	0x99AA,	0x2F,
	0x99AB,	0x2F,
	0x99AC,	0x2F,
	0x99AD,	0x00,
	0x99AE,	0x00,
	0x99AF,	0x00,
	0x99B0,	0x40,
	0x99B1,	0x40,
	0x99B2,	0x40,
	0x99B3,	0x30,
	0x99B4,	0x30,
	0x99B5,	0x30,
	0x99BB,	0x0A,
	0x99BD,	0x0A,
	0x99BF,	0x0A,
	0x99C0,	0x09,
	0x99C1,	0x09,
	0x99C2,	0x09,
	0x99C6,	0x3C,
	0x99C7,	0x3C,
	0x99C8,	0x3C,
	0x99C9,	0xFF,
	0x99CA,	0xFF,
	0x99CB,	0xFF,
	0x9A01,	0x35,
	0x9A03,	0x14,
	0x9A05,	0x14,
	0x9A07,	0x31,
	0x9A09,	0x1B,
	0x9A0B,	0x15,
	0x9A0D,	0x1E,
	0x9A0F,	0x1E,
	0x9A11,	0x1E,
	0x9A13,	0x64,
	0x9A15,	0x64,
	0x9A17,	0x64,
	0x9A19,	0x50,
	0x9A1B,	0x60,
	0x9A1D,	0x65,
	0x9A1F,	0x01,
	0x9A21,	0x01,
	0x9A23,	0x01,
	0x9A25,	0x14,
	0x9A27,	0x14,
	0x9A29,	0x14,
	0x9A2B,	0x2F,
	0x9A2D,	0x1A,
	0x9A2F,	0x14,
	0x9A31,	0x1E,
	0x9A33,	0x1E,
	0x9A35,	0x1E,
	0x9A37,	0x6B,
	0x9A39,	0x7C,
	0x9A3B,	0x81,
	0x9A3D,	0x00,
	0x9A3F,	0x00,
	0x9A41,	0x00,
	0x9A4F,	0x42,
	0x9A51,	0x0F,
	0x9A53,	0x0B,
	0x9A55,	0x5A,
	0x9A57,	0x13,
	0x9A59,	0x0C,
	0x9A5A,	0x00,
	0x9A5B,	0x00,
	0x9A5C,	0x00,
	0x9A6B,	0x00,
	0x9A6D,	0x10,
	0x9A6F,	0x10,
	0x9A71,	0xC8,
	0x9A73,	0x32,
	0x9A75,	0x04,
	0x9AA4,	0x3F,
	0x9AA5,	0x3F,
	0x9AA6,	0x3F,
	0x9AA7,	0x0A,
	0x9AA8,	0x0A,
	0x9AA9,	0x0A,
	0x9AAA,	0x3F,
	0x9AAB,	0x3F,
	0x9AAC,	0x3F,
	0x9AAD,	0x00,
	0x9AAE,	0x00,
	0x9AAF,	0x00,
	0x9AB0,	0x40,
	0x9AB1,	0x40,
	0x9AB2,	0x40,
	0x9AB3,	0x30,
	0x9AB4,	0x30,
	0x9AB5,	0x30,
	0x9AB6,	0xA0,
	0x9AB7,	0xA0,
	0x9AB8,	0xA0,
	0x9ABB,	0x0A,
	0x9ABD,	0x0A,
	0x9ABF,	0x0A,
	0x9AC0,	0x09,
	0x9AC1,	0x09,
	0x9AC2,	0x09,
	0x9AC6,	0x2D,
	0x9AC7,	0x2D,
	0x9AC8,	0x2D,
	0x9AC9,	0xFF,
	0x9ACA,	0xFF,
	0x9ACB,	0xFF,
	0x9B01,	0x35,
	0x9B03,	0x14,
	0x9B05,	0x14,
	0x9B07,	0x31,
	0x9B09,	0x1B,
	0x9B0B,	0x15,
	0x9B0D,	0x1E,
	0x9B0F,	0x1E,
	0x9B11,	0x1E,
	0x9B13,	0x64,
	0x9B15,	0x64,
	0x9B17,	0x64,
	0x9B19,	0x50,
	0x9B1B,	0x60,
	0x9B1D,	0x65,
	0x9B1F,	0x01,
	0x9B21,	0x01,
	0x9B23,	0x01,
	0x9B25,	0x14,
	0x9B27,	0x14,
	0x9B29,	0x14,
	0x9B2B,	0x2F,
	0x9B2D,	0x1A,
	0x9B2F,	0x14,
	0x9B31,	0x1E,
	0x9B33,	0x1E,
	0x9B35,	0x1E,
	0x9B37,	0x6B,
	0x9B39,	0x7C,
	0x9B3B,	0x81,
	0x9B43,	0x0F,
	0x9B45,	0x0F,
	0x9B47,	0x0F,
	0x9B49,	0x0F,
	0x9B4B,	0x0F,
	0x9B4D,	0x0F,
	0x9B4F,	0x2D,
	0x9B51,	0x0B,
	0x9B53,	0x08,
	0x9B55,	0x40,
	0x9B57,	0x0D,
	0x9B59,	0x08,
	0x9B5A,	0x00,
	0x9B5B,	0x00,
	0x9B5C,	0x00,
	0x9B6B,	0x00,
	0x9B6D,	0x10,
	0x9B6F,	0x10,
	0x9B71,	0xC8,
	0x9B73,	0x32,
	0x9B75,	0x04,
	0x9BB0,	0x40,
	0x9BB1,	0x40,
	0x9BB2,	0x40,
	0x9BB3,	0x30,
	0x9BB4,	0x30,
	0x9BB5,	0x30,
	0x9BBB,	0x0A,
	0x9BBD,	0x0A,
	0x9BBF,	0x0A,
	0x9BC0,	0x09,
	0x9BC1,	0x09,
	0x9BC2,	0x09,
	0x9BC6,	0x18,
	0x9BC7,	0x18,
	0x9BC8,	0x18,
	0x9BC9,	0xFF,
	0x9BCA,	0xFF,
	0x9BCB,	0xFF,
	0x9C01,	0x35,
	0x9C03,	0x14,
	0x9C05,	0x14,
	0x9C07,	0x31,
	0x9C09,	0x1B,
	0x9C0B,	0x15,
	0x9C0D,	0x1E,
	0x9C0F,	0x1E,
	0x9C11,	0x1E,
	0x9C13,	0x64,
	0x9C15,	0x64,
	0x9C17,	0x64,
	0x9C19,	0x50,
	0x9C1B,	0x60,
	0x9C1D,	0x65,
	0x9C1F,	0x01,
	0x9C21,	0x01,
	0x9C23,	0x01,
	0x9C25,	0x14,
	0x9C27,	0x14,
	0x9C29,	0x14,
	0x9C2B,	0x2F,
	0x9C2D,	0x1A,
	0x9C2F,	0x14,
	0x9C31,	0x1E,
	0x9C33,	0x1E,
	0x9C35,	0x1E,
	0x9C37,	0x6B,
	0x9C39,	0x7C,
	0x9C3B,	0x81,
	0x9C3D,	0x00,
	0x9C3F,	0x00,
	0x9C41,	0x00,
	0x9C4F,	0x2D,
	0x9C51,	0x0B,
	0x9C53,	0x08,
	0x9C55,	0x40,
	0x9C57,	0x0D,
	0x9C59,	0x08,
	0x9C5A,	0x00,
	0x9C5B,	0x00,
	0x9C5C,	0x00,
	0x9C6B,	0x00,
	0x9C6D,	0x10,
	0x9C6F,	0x10,
	0x9C71,	0xC8,
	0x9C73,	0x32,
	0x9C75,	0x04,
	0x9CB0,	0x50,
	0x9CB1,	0x50,
	0x9CB2,	0x50,
	0x9CB3,	0x40,
	0x9CB4,	0x40,
	0x9CB5,	0x40,
	0x9CBB,	0x0A,
	0x9CBD,	0x0A,
	0x9CBF,	0x0A,
	0x9CC0,	0x09,
	0x9CC1,	0x09,
	0x9CC2,	0x09,
	0x9CC6,	0x18,
	0x9CC7,	0x18,
	0x9CC8,	0x18,
	0x9CC9,	0xFF,
	0x9CCA,	0xFF,
	0x9CCB,	0xFF,
	0x9D01,	0x14,
	0x9D03,	0x14,
	0x9D05,	0x14,
	0x9D07,	0x31,
	0x9D09,	0x1B,
	0x9D0B,	0x15,
	0x9D0D,	0x1E,
	0x9D0F,	0x1E,
	0x9D11,	0x1E,
	0x9D13,	0x64,
	0x9D15,	0x64,
	0x9D17,	0x64,
	0x9D19,	0x50,
	0x9D1B,	0x60,
	0x9D1D,	0x65,
	0x9D25,	0x14,
	0x9D27,	0x14,
	0x9D29,	0x14,
	0x9D2B,	0x2F,
	0x9D2D,	0x1A,
	0x9D2F,	0x14,
	0x9D31,	0x1E,
	0x9D33,	0x1E,
	0x9D35,	0x1E,
	0x9D37,	0x6B,
	0x9D39,	0x7C,
	0x9D3B,	0x81,
	0x9D3D,	0x00,
	0x9D3F,	0x00,
	0x9D41,	0x00,
	0x9D4F,	0x42,
	0x9D51,	0x0F,
	0x9D53,	0x0B,
	0x9D55,	0x5A,
	0x9D57,	0x13,
	0x9D59,	0x0C,
	0x9D5B,	0x14,
	0x9D5D,	0x14,
	0x9D5F,	0x14,
	0x9D61,	0x14,
	0x9D63,	0x14,
	0x9D65,	0x14,
	0x9D67,	0x31,
	0x9D69,	0x1B,
	0x9D6B,	0x15,
	0x9D6D,	0x31,
	0x9D6F,	0x1B,
	0x9D71,	0x15,
	0x9D73,	0x1E,
	0x9D75,	0x1E,
	0x9D77,	0x1E,
	0x9D79,	0x1E,
	0x9D7B,	0x1E,
	0x9D7D,	0x1E,
	0x9D7F,	0x64,
	0x9D81,	0x64,
	0x9D83,	0x64,
	0x9D85,	0x64,
	0x9D87,	0x64,
	0x9D89,	0x64,
	0x9D8B,	0x50,
	0x9D8D,	0x60,
	0x9D8F,	0x65,
	0x9D91,	0x50,
	0x9D93,	0x60,
	0x9D95,	0x65,
	0x9D97,	0x01,
	0x9D99,	0x01,
	0x9D9B,	0x01,
	0x9D9D,	0x01,
	0x9D9F,	0x01,
	0x9DA1,	0x01,
	0x9E01,	0x35,
	0x9E03,	0x14,
	0x9E05,	0x14,
	0x9E07,	0x31,
	0x9E09,	0x1B,
	0x9E0B,	0x15,
	0x9E0D,	0x1E,
	0x9E0F,	0x1E,
	0x9E11,	0x1E,
	0x9E13,	0x64,
	0x9E15,	0x64,
	0x9E17,	0x64,
	0x9E19,	0x50,
	0x9E1B,	0x60,
	0x9E1D,	0x65,
	0x9E25,	0x14,
	0x9E27,	0x14,
	0x9E29,	0x14,
	0x9E2B,	0x2F,
	0x9E2D,	0x1A,
	0x9E2F,	0x14,
	0x9E31,	0x1E,
	0x9E33,	0x1E,
	0x9E35,	0x1E,
	0x9E37,	0x6B,
	0x9E39,	0x7C,
	0x9E3B,	0x81,
	0x9E3D,	0x00,
	0x9E3F,	0x00,
	0x9E41,	0x00,
	0x9E4F,	0x2D,
	0x9E51,	0x0B,
	0x9E53,	0x08,
	0x9E55,	0x40,
	0x9E57,	0x0D,
	0x9E59,	0x08,
	0x9E5B,	0x35,
	0x9E5D,	0x14,
	0x9E5F,	0x14,
	0x9E61,	0x35,
	0x9E63,	0x14,
	0x9E65,	0x14,
	0x9E67,	0x31,
	0x9E69,	0x1B,
	0x9E6B,	0x15,
	0x9E6D,	0x31,
	0x9E6F,	0x1B,
	0x9E71,	0x15,
	0x9E73,	0x1E,
	0x9E75,	0x1E,
	0x9E77,	0x1E,
	0x9E79,	0x1E,
	0x9E7B,	0x1E,
	0x9E7D,	0x1E,
	0x9E7F,	0x64,
	0x9E81,	0x64,
	0x9E83,	0x64,
	0x9E85,	0x64,
	0x9E87,	0x64,
	0x9E89,	0x64,
	0x9E8B,	0x50,
	0x9E8D,	0x60,
	0x9E8F,	0x65,
	0x9E91,	0x50,
	0x9E93,	0x60,
	0x9E95,	0x65,
	0x9E97,	0x01,
	0x9E99,	0x01,
	0x9E9B,	0x01,
	0x9E9D,	0x01,
	0x9E9F,	0x01,
	0x9EA1,	0x01,
	0x9F01,	0x14,
	0x9F03,	0x14,
	0x9F05,	0x14,
	0x9F07,	0x14,
	0x9F09,	0x14,
	0x9F0B,	0x14,
	0x9F0D,	0x2F,
	0x9F0F,	0x1A,
	0x9F11,	0x14,
	0x9F13,	0x2F,
	0x9F15,	0x1A,
	0x9F17,	0x14,
	0x9F19,	0x1E,
	0x9F1B,	0x1E,
	0x9F1D,	0x1E,
	0x9F1F,	0x1E,
	0x9F21,	0x1E,
	0x9F23,	0x1E,
	0x9F25,	0x6B,
	0x9F27,	0x7C,
	0x9F29,	0x81,
	0x9F2B,	0x6B,
	0x9F2D,	0x7C,
	0x9F2F,	0x81,
	0x9F31,	0x00,
	0x9F33,	0x00,
	0x9F35,	0x00,
	0x9F37,	0x00,
	0x9F39,	0x00,
	0x9F3B,	0x00,
	0x9F3C,	0x00,
	0x9F3D,	0x00,
	0x9F3E,	0x00,
	0x9F41,	0x00,
	0x9F43,	0x10,
	0x9F45,	0x10,
	0x9F47,	0xC8,
	0x9F49,	0x32,
	0x9F4B,	0x04,
	0x9F4D,	0x10,
	0x9F4F,	0x10,
	0x9F51,	0x10,
	0x9F53,	0x10,
	0x9F55,	0x10,
	0x9F57,	0x10,
	0x9F59,	0x04,
	0x9F5B,	0x04,
	0x9F5D,	0x04,
	0x9F5F,	0x04,
	0x9F61,	0x04,
	0x9F63,	0x04,
	0x9F77,	0x42,
	0x9F79,	0x0F,
	0x9F7B,	0x0B,
	0x9F7D,	0x42,
	0x9F7F,	0x0F,
	0x9F81,	0x0B,
	0x9F83,	0x5A,
	0x9F85,	0x13,
	0x9F87,	0x0C,
	0x9F89,	0x5A,
	0x9F8B,	0x13,
	0x9F8D,	0x0C,
	0x9FA6,	0x3F,
	0x9FA7,	0x3F,
	0x9FA8,	0x3F,
	0x9FA9,	0x0A,
	0x9FAA,	0x0A,
	0x9FAB,	0x0A,
	0x9FAC,	0x3F,
	0x9FAD,	0x3F,
	0x9FAE,	0x3F,
	0x9FAF,	0x00,
	0x9FB0,	0x00,
	0x9FB1,	0x00,
	0xA001,	0x14,
	0xA003,	0x14,
	0xA005,	0x14,
	0xA007,	0x14,
	0xA009,	0x14,
	0xA00B,	0x14,
	0xA00D,	0x2F,
	0xA00F,	0x1A,
	0xA011,	0x14,
	0xA013,	0x2F,
	0xA015,	0x1A,
	0xA017,	0x14,
	0xA019,	0x1E,
	0xA01B,	0x1E,
	0xA01D,	0x1E,
	0xA01F,	0x1E,
	0xA021,	0x1E,
	0xA023,	0x1E,
	0xA025,	0x6B,
	0xA027,	0x7C,
	0xA029,	0x81,
	0xA02B,	0x6B,
	0xA02D,	0x7C,
	0xA02F,	0x81,
	0xA031,	0x00,
	0xA033,	0x00,
	0xA035,	0x00,
	0xA037,	0x00,
	0xA039,	0x00,
	0xA03B,	0x00,
	0xA03C,	0x00,
	0xA03D,	0x00,
	0xA03E,	0x00,
	0xA041,	0x00,
	0xA043,	0x10,
	0xA045,	0x10,
	0xA047,	0xC8,
	0xA049,	0x32,
	0xA04B,	0x04,
	0xA04D,	0x10,
	0xA04F,	0x10,
	0xA051,	0x10,
	0xA053,	0x10,
	0xA055,	0x10,
	0xA057,	0x10,
	0xA059,	0x04,
	0xA05B,	0x04,
	0xA05D,	0x04,
	0xA05F,	0x04,
	0xA061,	0x04,
	0xA063,	0x04,
	0xA077,	0x2D,
	0xA079,	0x0B,
	0xA07B,	0x08,
	0xA07D,	0x2D,
	0xA07F,	0x0B,
	0xA081,	0x08,
	0xA083,	0x40,
	0xA085,	0x0D,
	0xA087,	0x08,
	0xA089,	0x40,
	0xA08B,	0x0D,
	0xA08D,	0x08,
	0xA716,	0x13,
	0xA801,	0x08,
	0xA803,	0x0C,
	0xA805,	0x10,
	0xA806,	0x00,
	0xA807,	0x18,
	0xA808,	0x00,
	0xA809,	0x20,
	0xA80A,	0x00,
	0xA80B,	0x30,
	0xA80C,	0x00,
	0xA80D,	0x40,
	0xA80E,	0x00,
	0xA80F,	0x60,
	0xA810,	0x00,
	0xA811,	0x80,
	0xA812,	0x00,
	0xA813,	0xC0,
	0xA814,	0x01,
	0xA815,	0x00,
	0xA816,	0x01,
	0xA817,	0x80,
	0xA818,	0x02,
	0xA819,	0x00,
	0xA81A,	0x03,
	0xA81B,	0x00,
	0xA81C,	0x03,
	0xA81D,	0xAC,
	0xA838,	0x03,
	0xA83C,	0x28,
	0xA83D,	0x5F,
	0xA881,	0x08,
	0xA883,	0x0C,
	0xA885,	0x10,
	0xA886,	0x00,
	0xA887,	0x18,
	0xA888,	0x00,
	0xA889,	0x20,
	0xA88A,	0x00,
	0xA88B,	0x30,
	0xA88C,	0x00,
	0xA88D,	0x40,
	0xA88E,	0x00,
	0xA88F,	0x60,
	0xA890,	0x00,
	0xA891,	0x80,
	0xA892,	0x00,
	0xA893,	0xC0,
	0xA894,	0x01,
	0xA895,	0x00,
	0xA896,	0x01,
	0xA897,	0x80,
	0xA898,	0x02,
	0xA899,	0x00,
	0xA89A,	0x03,
	0xA89B,	0x00,
	0xA89C,	0x03,
	0xA89D,	0xAC,
	0xA8B8,	0x03,
	0xA8BB,	0x13,
	0xA8BC,	0x28,
	0xA8BD,	0x25,
	0xA8BE,	0x1D,
	0xA8C0,	0x3A,
	0xA8C1,	0xE0,
	0xB040,	0x90,
	0xB041,	0x14,
	0xB042,	0x6B,
	0xB043,	0x43,
	0xB044,	0x90,
	0xB045,	0x0E,
	0xB24F,	0x80,
};

static void sensor_init(void)
{
	pr_debug("E\n");
	imx338_table_write_cmos_sensor(addr_data_pair_init_imx338,
	    sizeof(addr_data_pair_init_imx338)/sizeof(kal_uint16));

	/*enable temperature sensor, TEMP_SEN_CTL:*/
	write_cmos_sensor(0x0138, 0x01);
} /* sensor_init  */


kal_uint16 addr_data_pair_preview_imx338[] = {
	0x0100, 0x00,

	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x0b,
	0x0341, 0x50,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0F,
	0x034B, 0xAF,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x02,
	0x3000, 0x74,
	0x3001, 0x00,
	0x305C, 0x11,

	0x0112, 0x0A,
	0x0113, 0x0A,
	0x034C, 0x0A,
	0x034D, 0x70,
	0x034E, 0x07,
	0x034F, 0xD8,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0A,
	0x040D, 0x70,
	0x040E, 0x07,
	0x040F, 0xD8,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB1,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0F,
	0x030E, 0x01,
	0x030F, 0xEF,
	0x0310, 0x01,

	0x0820, 0x11,
	0x0821, 0x2C,
	0x0822, 0xCC,
	0x0823, 0xCC,

	0x0202, 0x0A,
	0x0203, 0xE6,
	0x0224, 0x01,
	0x0225, 0xF4,

	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	0x3006, 0x01,
	0x3007, 0x02,
	0x31E0, 0x03,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A22, 0x00,
	0x6957, 0x00,
	0x69CD, 0x19,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
	/* 0x0100, 0x01 */
};

kal_uint16 addr_data_pair_preview_imx338_hdr[] = {
	0x0100,	0x00,

	0x0114, 0x03,
	0x0220, 0x23,
	0x0221, 0x22,
	0x0222, 0x10,
	0x0340, 0x0b,
	0x0341, 0x50,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0F,
	0x034B, 0xAF,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x02,
	0x3000, 0x75,
	0x3001, 0x00,
	0x305C, 0x11,

	0x0112, 0x0A,
	0x0113, 0x0A,
	0x034C, 0x0A,
	0x034D, 0x70,
	0x034E, 0x07,
	0x034F, 0xD8,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0A,
	0x040D, 0x70,
	0x040E, 0x07,
	0x040F, 0xD8,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB1,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0F,
	0x030E, 0x01,
	0x030F, 0xEF,
	0x0310, 0x01,

	0x0820, 0x11,
	0x0821, 0x2c,
	0x0822, 0xcc,
	0x0823, 0xcc,

	0x0202, 0x0a,
	0x0203, 0xe6,
	0x0224, 0x00,
	0x0225, 0xae,

	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	0x3006, 0x01,
	0x3007, 0x00,
	0x31E0, 0x3f,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A22, 0x10,
	0x6957, 0x00,
	0x69CD, 0x19,

	0x3A21, 0x02,

	0x3011, 0x00,
	0x3013, 0x01,
};

static void preview_setting(void)
{
	if ((imgsensor.hdr_mode == 2) || (imgsensor.hdr_mode == 9)) {
		imx338_table_write_cmos_sensor(
		addr_data_pair_preview_imx338_hdr,
		sizeof(addr_data_pair_preview_imx338_hdr) / sizeof(kal_uint16));

		imx338_ATR(3, 3);
		zvhdr_setting();
	} else {
		imx338_table_write_cmos_sensor(
		addr_data_pair_preview_imx338,
		sizeof(addr_data_pair_preview_imx338) / sizeof(kal_uint16));

		zvhdr_setting();
	}
}				/*    preview_setting  */

static void custom1_setting(void)
{
	preview_setting();
	zvhdr_setting();
}

static void custom2_setting(void)
{
	preview_setting();
	zvhdr_setting();
}

static void custom3_setting(void)
{
	preview_setting();
	zvhdr_setting();
}

static void custom4_setting(void)
{
	preview_setting();
	zvhdr_setting();
}

static void custom5_setting(void)
{
	preview_setting();
	zvhdr_setting();
}

kal_uint16 addr_data_pair_capture_imx338[] = {
	0x0100, 0x00,

	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x10,
	0x0341, 0x36,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0F,
	0x034B, 0xAF,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x3000, 0x74,
	0x3001, 0x00,
	0x305C, 0x11,

	0x0112, 0x0A,
	0x0113, 0x0A,
	0x034C, 0x14,
	0x034D, 0xE0,
	0x034E, 0x0F,
	0x034F, 0xB0,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x14,
	0x040D, 0xE0,
	0x040E, 0x0F,
	0x040F, 0xB0,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0F,
	0x030E, 0x03,
	0x030F, 0xA9,
	0x0310, 0x01,

	0x0820, 0x17,
	0x0821, 0x6C,
	0x0822, 0xCC,
	0x0823, 0xCC,

	0x0202, 0x10,
	0x0203, 0x2C,
	0x0224, 0x01,
	0x0225, 0xF4,

	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	0x3006, 0x01,
	0x3007, 0x02,
	0x31E0, 0x03,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A22, 0x00,
	0x6957, 0x00,
	0x69CD, 0x19,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,

    /* ,0x0100 ,0x01 */
};

kal_uint16 addr_data_pair_capture_imx338_hdr[] = {
	0x0100, 0x00,
	0x0114, 0x03,
	0x0220, 0x23,
	0x0221, 0x11,
	0x0222, 0x10,
	0x0340, 0x10,
	0x0341, 0x36,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0F,
	0x034B, 0xAF,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x3000, 0x75,
	0x3001, 0x00,
	0x305C, 0x11,

	0x0112, 0x0A,
	0x0113, 0x0A,
	0x034C, 0x14,
	0x034D, 0xE0,
	0x034E, 0x0F,
	0x034F, 0xB0,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x14,
	0x040D, 0xE0,
	0x040E, 0x0F,
	0x040F, 0xB0,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0F,
	0x030E, 0x03,
	0x030F, 0xA9,
	0x0310, 0x01,

	0x0820, 0x17,
	0x0821, 0x6C,
	0x0822, 0xCC,
	0x0823, 0xCC,

	0x0202, 0x10,
	0x0203, 0x2C,
	0x0224, 0x01,
	0x0225, 0x02,

	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	0x3006, 0x01,
	0x3007, 0x00,
	0x31E0, 0x3F,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A22, 0x00,
	0x6957, 0x02,
	0x69CD, 0x1E,

	0x3A21, 0x02,

	0x3011, 0x00,
	0x3013, 0x01,
/* ,0x0100 ,0x01 */
};

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x0101, 0x03);
		write_cmos_sensor(0x0100, 0X01);
	} else
		write_cmos_sensor(0x0100, 0x00);
	return ERROR_NONE;
}


kal_uint16 addr_data_pair_capture_imx338_pdaf_on[] = {
	0x3001, 0x01,/*bit[0]PDAF enable during HDR on */
	/*PDAF*/
	/*PD_CAL_ENALBE */
	0x3121, 0x01,
	/*AREA MODE */
	0x31B0, 0x01,
	/*PD_OUT_EN=1 */
	0x3123, 0x01,
	/*Fixed area mode */
	0x3150, 0x00,
	0x3151, 0x70,
	0x3152, 0x00,
	0x3153, 0x58,
	0x3154, 0x02,
	0x3155, 0x80,
	0x3156, 0x02,
	0x3157, 0x80,
};

static void capture_setting(kal_uint16 currefps)
{
	pr_debug("E! currefps:%d hdr:%d pdaf:%d\n",
		currefps, imgsensor.hdr_mode, imgsensor.pdaf_mode);
	if ((imgsensor.hdr_mode == 2) || (imgsensor.hdr_mode == 9)) {
		imx338_table_write_cmos_sensor(
		addr_data_pair_capture_imx338_hdr,
		sizeof(addr_data_pair_capture_imx338_hdr) / sizeof(kal_uint16));

		zvhdr_setting();
	} else {
		imx338_table_write_cmos_sensor(addr_data_pair_capture_imx338,
		    sizeof(addr_data_pair_capture_imx338) / sizeof(kal_uint16));

		zvhdr_setting();
	}
	if (imgsensor.pdaf_mode == 1) {
		imx338_table_write_cmos_sensor(
		addr_data_pair_capture_imx338_pdaf_on,
	    sizeof(addr_data_pair_capture_imx338_pdaf_on) / sizeof(kal_uint16));

		imx338_apply_SPC();
	}

}

kal_uint16 addr_data_pair_video_imx338[] = {
	0x0100, 0x00,
	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x0C,
	0x0341, 0xE8,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0xF8,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0D,
	0x034B, 0xB7,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x3000, 0x74,
	0x3001, 0x00,
	0x305C, 0x11,

	0x0112, 0x0A,
	0x0113, 0x0A,
	0x034C, 0x14,
	0x034D, 0xE0,
	0x034E, 0x0B,
	0x034F, 0xC0,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x14,
	0x040D, 0xE0,
	0x040E, 0x0B,
	0x040F, 0xC0,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0F,
	0x030E, 0x03,
	0x030F, 0xA9,
	0x0310, 0x01,

	0x0820, 0x17,
	0x0821, 0x6C,
	0x0822, 0xCC,
	0x0823, 0xCC,

	0x0202, 0x0C,
	0x0203, 0xDE,
	0x0224, 0x01,
	0x0225, 0xF4,

	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	0x3006, 0x01,
	0x3007, 0x02,
	0x31E0, 0x03,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A22, 0x00,
	0x6957, 0x00,
	0x69CD, 0x19,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,

	/* ,0x0100 ,0x01 */
};


kal_uint16 addr_data_pair_video_imx338_hdr[] = {
	0x0100, 0x00,
	0x0114, 0x03,
	0x0220, 0x23,
	0x0221, 0x11,
	0x0222, 0x10,
	0x0340, 0x0C,
	0x0341, 0xE8,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0xF8,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0D,
	0x034B, 0xB7,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x3000, 0x75,
	0x3001, 0x00,
	0x305C, 0x11,

	0x0112, 0x0A,
	0x0113, 0x0A,
	0x034C, 0x14,
	0x034D, 0xE0,
	0x034E, 0x0B,
	0x034F, 0xC0,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x14,
	0x040D, 0xE0,
	0x040E, 0x0B,
	0x040F, 0xC0,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0F,
	0x030E, 0x03,
	0x030F, 0xA9,
	0x0310, 0x01,

	0x0820, 0x17,
	0x0821, 0x6C,
	0x0822, 0xCC,
	0x0823, 0xCC,

	0x0202, 0x0C,
	0x0203, 0xDE,
	0x0224, 0x00,
	0x0225, 0xCD,

	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	0x3006, 0x01,
	0x3007, 0x00,
	0x31E0, 0x3F,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A22, 0x00,
	0x6957, 0x02,
	0x69CD, 0x1E,

	0x3A21, 0x02,

	0x3011, 0x00,
	0x3013, 0x01,

	/* ,0x0100 ,0x01 */
};

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("E! %s:%d\n", __func__, currefps);
	if ((imgsensor.hdr_mode == 2) || (imgsensor.hdr_mode == 9)) {
		imx338_table_write_cmos_sensor(addr_data_pair_video_imx338_hdr,
		  sizeof(addr_data_pair_video_imx338_hdr) / sizeof(kal_uint16));

		zvhdr_setting();
	} else {
		imx338_table_write_cmos_sensor(addr_data_pair_video_imx338,
		    sizeof(addr_data_pair_video_imx338) / sizeof(kal_uint16));

		zvhdr_setting();
	}
}

kal_uint16 addr_data_pair_hs_video_imx338[] = {	/*720 120fps */
	0x0100, 0x00,

	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x03,
	0x0341, 0x3E,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0x18,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0D,
	0x034B, 0x97,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x02,
	0x3000, 0x74,
	0x3001, 0x00,
	0x305C, 0x11,

	0x0112, 0x0A,
	0x0113, 0x0A,
	0x034C, 0x05,
	0x034D, 0x10,
	0x034E, 0x02,
	0x034F, 0xE0,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x14,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x05,
	0x040D, 0x10,
	0x040E, 0x02,
	0x040F, 0xE0,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x02,
	0x030D, 0x0F,
	0x030E, 0x02,
	0x030F, 0xEE,
	0x0310, 0x01,

	0x0820, 0x09,
	0x0821, 0x60,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x03,
	0x0203, 0x34,
	0x0224, 0x01,
	0x0225, 0xF4,

	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	0x3006, 0x01,
	0x3007, 0x02,
	0x31E0, 0x03,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A22, 0x00,
	0x6957, 0x00,
	0x69CD, 0x19,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,

	/* ,0x0100 ,0x01 */
};

static void hs_video_setting(void)
{
	imx338_table_write_cmos_sensor(addr_data_pair_hs_video_imx338,
		sizeof(addr_data_pair_hs_video_imx338) / sizeof(kal_uint16));
	zvhdr_setting();
}

kal_uint16 addr_data_pair_slim_video_imx338[] = {
	0x0100, 0x00,

	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x05,
	0x0341, 0xBE,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0x18,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0D,
	0x034B, 0x97,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x02,
	0x3000, 0x74,
	0x3001, 0x00,
	0x305C, 0x11,

	0x0112, 0x0A,
	0x0113, 0x0A,
	0x034C, 0x05,
	0x034D, 0x10,
	0x034E, 0x02,
	0x034F, 0xE0,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x14,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x05,
	0x040D, 0x10,
	0x040E, 0x02,
	0x040F, 0xE0,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB1,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0F,
	0x030E, 0x01,
	0x030F, 0xEF,
	0x0310, 0x01,

	0x0820, 0x0C,
	0x0821, 0x60,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x05,
	0x0203, 0xB4,
	0x0224, 0x01,
	0x0225, 0xF4,

	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	0x3006, 0x01,
	0x3007, 0x02,
	0x31E0, 0x03,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A22, 0x00,
	0x6957, 0x00,
	0x69CD, 0x19,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
	/* ,0x0100 ,0x01 */

};

static void slim_video_setting(void)
{
	/* @@video_720p_60fps */
	imx338_table_write_cmos_sensor(addr_data_pair_slim_video_imx338,
	    sizeof(addr_data_pair_slim_video_imx338) / sizeof(kal_uint16));

	zvhdr_setting();
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0601, 0x02);
	else
		write_cmos_sensor(0x0601, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
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
				read_imx338_SPC(imx338_SPC_data);
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

	pr_debug("IMX338,MIPI 4LANE\n");
	pr_debug(
	 "preview 2672*2008@30fps; video 5344*4016@30fps; capture 21M@24fps\n");


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
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	KD_SENSOR_PROFILE("open_2");
	return ERROR_NONE;
} /*    open  */



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
} /*    close  */


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
} /*    preview   */

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

	KD_SENSOR_PROFILE_INIT();

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
			pr_debug(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	KD_SENSOR_PROFILE("cap_lock");

	capture_setting(imgsensor.current_fps);	/*Full mode */

	KD_SENSOR_PROFILE("cap_setting");

	return ERROR_NONE;
} /* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

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
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
} /*    normal_video   */

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
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    hs_video   */

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
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
} /*    slim_video     */



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

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;

	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;

	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width =
		imgsensor_info.custom3.grabwindow_width;

	sensor_resolution->SensorCustom3Height =
		imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.custom4.grabwindow_width;

	sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width =
		imgsensor_info.custom5.grabwindow_width;

	sensor_resolution->SensorCustom5Height =
		imgsensor_info.custom5.grabwindow_height;

	return ERROR_NONE;
}				/*    get_resolution    */

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
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
#if defined(IMX338_ZHDR)

	/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR */
	sensor_info->HDR_Support = 3;
	/*0: no support, 1: G0,R0.B0, 2: G0,R0.B1, 3: G0,R1.B0, 4: G0,R1.B1 */
	/*5: G1,R0.B0, 6: G1,R0.B1, 7: G1,R1.B0, 8: G1,R1.B1 */
	sensor_info->ZHDR_Mode = 8;
#else
	sensor_info->HDR_Support = 2;/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR */
#endif
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
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

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	return ERROR_NONE;
}

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	return ERROR_NONE;
}

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	return ERROR_NONE;
}

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();
	return ERROR_NONE;
}				/*  Custom4   */


static kal_uint32 Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom5_setting();
	return ERROR_NONE;
}				/*  Custom5   */

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
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		Custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		Custom5(image_window, sensor_config_data);
		break;
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
} /* control() */



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
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.pre.framelength)
		    ? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk
		      / framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		(frame_length > imgsensor_info.normal_video.framelength)
		 ? (frame_length - imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {

			frame_length = imgsensor_info.cap1.pclk
			    / framerate * 10 / imgsensor_info.cap1.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap1.framelength)
			? (frame_length - imgsensor_info.cap1.framelength) : 0;

			imgsensor.frame_length =
			 imgsensor_info.cap1.framelength + imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps
				!= imgsensor_info.cap.max_framerate)

				pr_debug(
				    "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				    framerate,
				    imgsensor_info.cap.max_framerate / 10);

			frame_length = imgsensor_info.cap.pclk
			       / framerate * 10 / imgsensor_info.cap.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;

			imgsensor.frame_length =
			  imgsensor_info.cap.framelength + imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.hs_video.framelength)
		    ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.slim_video.framelength)
		   ? (frame_length - imgsensor_info.slim_video.framelength) : 0;

		imgsensor.frame_length =
		   imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;

	/* coding with  preview scenario by default */
	default:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.pre.framelength)
		    ? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
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


static kal_uint32 imx338_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
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


static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB =
		(struct SET_SENSOR_AWB_GAIN *) feature_para;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*pr_debug("feature_id = %d\n", feature_id);*/
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
		night_mode((BOOL) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN:
		set_dual_gain(
		    (UINT16) *feature_data,
		    (UINT16) *(feature_data + 1));

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
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		read_imx338_DCC((kal_uint16) (*feature_data),
				(char *)(uintptr_t) (*(feature_data + 1)),
				(kal_uint32) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) (*feature_data));
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
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);

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
		/*HDR CMD */
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("hdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));

		hdr_write_shutter(
		    (UINT16) *feature_data,
		    (UINT16) *(feature_data + 1),
		    (UINT16) *(feature_data + 2));

		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		imx338_awb_gain(pSetSensorAWB);
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		pr_debug(
		    "SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu\n",
		    *feature_data);
		/*
		 * SENSOR_VHDR_MODE_NONE  = 0x0,
		 * SENSOR_VHDR_MODE_IVHDR = 0x01,
		 * SENSOR_VHDR_MODE_MVHDR = 0x02,
		 * SENSOR_VHDR_MODE_ZVHDR = 0x09
		 */
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x02;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x02;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x02;
			break;
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		}
		break;

		/*END OF HDR CMD */
		/*PDAF CMD */
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug(
		    "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n",
		    *feature_data);

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
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));

		imx338_get_pdaf_reg_setting(
			(*feature_para_len) / sizeof(UINT32), feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));

		imx338_set_pdaf_reg_setting(
			(*feature_para_len) / sizeof(UINT32), feature_data_16);
		break;

	case SENSOR_FEATURE_SET_PDFOCUS_AREA:
		pr_debug(
		    "SENSOR_FEATURE_SET_IMX338_PDFOCUS_AREA Start Pos=%d, Size=%d\n",
		    (UINT32) *feature_data, (UINT32) *(feature_data + 1));

		imx338_set_pd_focus_area(*feature_data, *(feature_data + 1));
		break;
		/*End of PDAF */
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
}				/*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX338_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    IMX230_MIPI_RAW_SensorInit    */
