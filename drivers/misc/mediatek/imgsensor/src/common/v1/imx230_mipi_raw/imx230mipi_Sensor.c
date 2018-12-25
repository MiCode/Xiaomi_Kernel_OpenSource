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

/*******************Modify Following Strings for Debug************************/
#define PFX "IMX230_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__
/****************************   Modify end    ********************************/


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

#include "imx230mipi_Sensor.h"



/*
 * #define LOG_INF(format, args...) pr_debug(\
 * PFX "[%s] " format, __func__, ##args)
 */

#define BYTE               unsigned char




static DEFINE_SPINLOCK(imgsensor_drv_lock);

static BYTE imx230_SPC_data[352] = { 0 };





static struct imgsensor_info_struct imgsensor_info = {

	/* record sensor id defined in Kd_imgsensor.h */
	.sensor_id = IMX230_SENSOR_ID,

	.checksum_value = 0xafd83a68, /* checksum value for Camera Auto Test */

	.pre = {
		.pclk = 381000000,	/* record different mode's pclk */
		.linelength = 6024,	/* record different mode's linelength */
		.framelength = 2108, /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2672,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 2008,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */

		/*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 300,
		.mipi_pixel_rate = 188000000,
	},
	.cap = {
		.pclk = 597000000,
		.linelength = 6024,
		.framelength = 4128,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5344,
		.grabwindow_height = 4016,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 240,
		.mipi_pixel_rate = 561920000,
	},
	.cap1 = {
		 .pclk = 375000000,
		 .linelength = 6024,
		 .framelength = 4126,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 5344,
		 .grabwindow_height = 4016,
		 .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		 .max_framerate = 150,
	},
	.normal_video = {
		.pclk = 561000000,
		.linelength = 6024,
		.framelength = 3104,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5344,
		.grabwindow_height = 3004,
		.mipi_data_lp2hs_settle_dc = 85,/* unit , ns */
		.max_framerate = 300,
		.mipi_pixel_rate = 528000000,
	},
	.hs_video = {
		.pclk = 600000000,
		.linelength = 6024,
		.framelength = 830,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 1200,
		.mipi_pixel_rate = 158400000,
	},
	.slim_video = {
		.pclk = 600000000,
		.linelength = 6024,
		.framelength = 3320,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 300,
		.mipi_pixel_rate = 158400000,
	},
	.margin = 4,		/* sensor framelength & shutter margin */
	.min_shutter = 1,	/* min shutter */

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0x7fff,

	/* shutter delay frame for AE cycle,
	 *2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,2 frame
	 *with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 1,	/* enter capture delay frame num */
	.pre_delay_frame = 1,	/* enter preview delay frame num */
	.video_delay_frame = 1,	/* enter video delay frame num */
	.hs_video_delay_frame = 3, /* enter high speed video  delay frame num */
	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_8MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,

	/* sensor output first pixel color */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,

	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */

	/* record sensor support all write id addr, end with 0xff */
	.i2c_addr_table = {0x34, 0x20, 0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT,
	 * Preview, Capture, Video,High Speed Video, Slim Video
	 */
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

	/* test pattern mode or not. KAL_FALSE for in test pattern mode,
	 * KAL_TRUE for normal output
	 */
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	.ihdr_mode = 0,	/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x6c,	/* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
{5344, 4016, 0, 0, 5344, 4016, 2672, 2008,
0000, 0000, 2672, 2008, 0, 0, 2672, 2008},		/* Preview */

{5344, 4016, 0, 0, 5344, 4016, 5344, 4016,
0000, 0000, 5344, 4016, 0, 0, 5344, 4016},		/* capture */

{5344, 4016, 0, 0, 5344, 3004, 5344, 3004,
0000, 0000, 5344, 3004, 0, 0, 5344, 3004},		/* video */

{5344, 4016, 0, 0, 5344, 4016, 1336, 1004,
0034, 0000, 1280, 720, 0, 0, 1280, 720},		/* hight speed video */

{5344, 4016, 0, 0, 5344, 4016, 1336, 1004,
0034, 0000, 1280, 720, 0, 0, 1280, 720} /* slim video */
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
static struct SENSOR_ATR_INFO sensorATR_Info[4] = {	/* Strength Range Min */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	/* Strength Range Std */
	{0x00, 0x32, 0x00, 0x3c, 0x03, 0xff},
	/* Strength Range Max */
	{0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xff},
	/* Strength Range Custom */
	{0x3F, 0xFF, 0x00, 0x0, 0x3F, 0xFF}
};
#endif

#define IMX230MIPI_MaxGainIndex (115)
kal_uint16 IMX230MIPI_sensorGainMapping[IMX230MIPI_MaxGainIndex][2] = {
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

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

#if 0
static kal_uint32 imx230_ATR(UINT16 DarkLimit, UINT16 OverExp)
{

	write_cmos_sensor(0x6e50, sensorATR_Info[DarkLimit].DarkLimit_H);
	write_cmos_sensor(0x6e51, sensorATR_Info[DarkLimit].DarkLimit_L);
	write_cmos_sensor(0x9340, sensorATR_Info[OverExp].OverExp_Min_H);
	write_cmos_sensor(0x9341, sensorATR_Info[OverExp].OverExp_Min_L);
	write_cmos_sensor(0x9342, sensorATR_Info[OverExp].OverExp_Max_H);
	write_cmos_sensor(0x9343, sensorATR_Info[OverExp].OverExp_Max_L);
	write_cmos_sensor(0x9706, 0x10);
	write_cmos_sensor(0x9707, 0x03);
	write_cmos_sensor(0x9708, 0x03);
	write_cmos_sensor(0x9e24, 0x00);
	write_cmos_sensor(0x9e25, 0x8c);
	write_cmos_sensor(0x9e26, 0x00);
	write_cmos_sensor(0x9e27, 0x94);
	write_cmos_sensor(0x9e28, 0x00);
	write_cmos_sensor(0x9e29, 0x96);
	pr_debug("DarkLimit 0x6e50(0x%x), 0x6e51(0x%x)\n",
		sensorATR_Info[DarkLimit].DarkLimit_H,
		sensorATR_Info[DarkLimit].DarkLimit_L);
	pr_debug("OverExpMin 0x9340(0x%x), 0x9341(0x%x)\n",
		sensorATR_Info[OverExp].OverExp_Min_H,
		sensorATR_Info[OverExp].OverExp_Min_L);
	pr_debug("OverExpMin 0x9342(0x%x), 0x9343(0x%x)\n",
		sensorATR_Info[OverExp].OverExp_Max_H,
		sensorATR_Info[OverExp].OverExp_Max_L);
	return ERROR_NONE;
}
#endif

#if 0
static MUINT32 cur_startpos;
static MUINT32 cur_size;

static void imx230_set_pd_focus_area(MUINT32 startpos, MUINT32 size)
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
	"start_x_pos:%d, start_y_pos:%d, focus_width:%d, focus_height:%d, end_x_pos:%d, end_y_pos:%d\n",
	     start_x_pos,
	     start_y_pos,
	     focus_width,
	     focus_height,
	     end_x_pos,
	     end_y_pos);

}
#endif

static void imx230_get_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor(regDa[idx]);
		/* pr_debug("%x %x", regDa[idx], regDa[idx+1]); */
	}
}

static void imx230_set_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor(regDa[idx], regDa[idx + 1]);
		/* pr_debug("%x %x", regDa[idx], regDa[idx+1]); */
	}
}


static void imx230_apply_SPC(void)
{
	unsigned int start_reg = 0x7c00;
	char puSendCmd[355];
	kal_uint32 tosend = 0;

	tosend = 0;
	puSendCmd[tosend++] = (char)(start_reg >> 8);
	puSendCmd[tosend++] = (char)(start_reg & 0xFF);
	memcpy((void *)&puSendCmd[tosend], imx230_SPC_data, 352);
	tosend += 352;
	iBurstWriteReg_multi(
	    puSendCmd,
	    tosend,
	    imgsensor.i2c_write_id,
	    tosend,
	    imgsensor_info.i2c_speed);

#if 0
    /* for verify */
	for (i = 0x7c00; i < 0x7c00+352; i++)
		pr_info(
		"SPC read out : Addr[%3d] Data[%x], Ref[%x]",
		i, read_cmos_sensor(i), imx230_SPC_data[i-0x7c00]);
#endif
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel,
	 *or set dummy by imgsensor.frame_length and imgsensor.line_length
	 */
	write_cmos_sensor(0x0104, 0x01);

	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);

	write_cmos_sensor(0x0104, 0x00);
}				/*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	write_cmos_sensor(0x0A02, 0x1F);
	write_cmos_sensor(0x0A00, 0x01);
	write_cmos_sensor(0x0A01, 0x01);

	return (
	(read_cmos_sensor(0x0A38) << 4) | (read_cmos_sensor(0x0A39) >> 4));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	pr_info("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

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
#define MAX_CIT_LSHIFT 7
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_uint16 l_shift = 1;

	pr_debug("Enter! shutter =%d, framelength =%d\n",
		shutter,
		imgsensor.frame_length);


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

	pr_info("[IMX230MIPI]enter IMX230MIPIGain2Reg function\n");
	for (iI = 0; iI < IMX230MIPI_MaxGainIndex; iI++) {
		if (gain < IMX230MIPI_sensorGainMapping[iI][0])
			return IMX230MIPI_sensorGainMapping[iI][1];

	}
	if (iI != IMX230MIPI_MaxGainIndex) {
		if (gain != IMX230MIPI_sensorGainMapping[iI][0]) {
			pr_info("Gain mapping don't correctly:%d %d\n", gain,
				IMX230MIPI_sensorGainMapping[iI][0]);
		}
	}
	pr_info("exit IMX230MIPIGain2Reg function\n");
	return IMX230MIPI_sensorGainMapping[iI - 1][1];
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

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < BASEGAIN || gain > 8 * BASEGAIN) {
		pr_info("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 8 * BASEGAIN)
			gain = 8 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_info("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

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

static void ihdr_write_shutter_gain(
	kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{

	kal_uint16 realtime_fps = 0;
	kal_uint16 reg_gain;
	kal_uint16 ratio;

	pr_info("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
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
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		      write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
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
	/* Ratio */
	if (se == 0)
		ratio = 2;
	else {
		ratio = (le + (se >> 1)) / se;
		if (ratio > 16)
			ratio = 2;
	}
	pr_info("le:%d, se:%d, ratio:%d\n", le, se, ratio);
	write_cmos_sensor(0x0222, ratio);

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	/* Global analog Gain for Long expo */
	write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
	write_cmos_sensor(0x0205, reg_gain & 0xFF);
	/* Global analog Gain for Short expo */
	write_cmos_sensor(0x0216, (reg_gain >> 8) & 0xFF);
	write_cmos_sensor(0x0217, reg_gain & 0xFF);
	write_cmos_sensor(0x0104, 0x00);

}

static void hdr_write_shutter(kal_uint16 le, kal_uint16 se)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 ratio;

	pr_info("le:0x%x, se:0x%x\n", le, se);
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
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		      write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
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

	pr_info("le:%d, se:%d, ratio:%d\n", le, se, ratio);
	write_cmos_sensor(0x0222, ratio);

}


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	pr_debug("image_mirror = %d\n", image_mirror);

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


#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3

#endif
static kal_uint16 imx230_table_write_cmos_sensor(
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

		if ((I2C_BUFFER_LEN - tosend) < 3  ||
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

kal_uint16 addr_data_pair_init_imx230[] = {

	0x0136, 0x18,
	0x0137, 0x00,

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
	0x9009, 0x1A,
	0xB040, 0x90,
	0xB041, 0x14,
	0xB042, 0x6B,
	0xB043, 0x43,
	0xB044, 0x63,
	0xB045, 0x2A,
	0xB046, 0x68,
	0xB047, 0x06,
	0xB048, 0x68,
	0xB049, 0x07,
	0xB04A, 0x68,
	0xB04B, 0x04,
	0xB04C, 0x68,
	0xB04D, 0x05,
	0xB04E, 0x68,
	0xB04F, 0x16,
	0xB050, 0x68,
	0xB051, 0x17,
	0xB052, 0x68,
	0xB053, 0x74,
	0xB054, 0x68,
	0xB055, 0x75,
	0xB056, 0x68,
	0xB057, 0x76,
	0xB058, 0x68,
	0xB059, 0x77,
	0xB05A, 0x68,
	0xB05B, 0x7A,
	0xB05C, 0x68,
	0xB05D, 0x7B,
	0xB05E, 0x68,
	0xB05F, 0x0A,
	0xB060, 0x68,
	0xB061, 0x0B,
	0xB062, 0x68,
	0xB063, 0x08,
	0xB064, 0x68,
	0xB065, 0x09,
	0xB066, 0x68,
	0xB067, 0x0E,
	0xB068, 0x68,
	0xB069, 0x0F,
	0xB06A, 0x68,
	0xB06B, 0x0C,
	0xB06C, 0x68,
	0xB06D, 0x0D,
	0xB06E, 0x68,
	0xB06F, 0x13,
	0xB070, 0x68,
	0xB071, 0x12,
	0xB072, 0x90,
	0xB073, 0x0E,
	0xD000, 0xDA,
	0xD001, 0xDA,
	0xD002, 0x7B,
	0xD003, 0x00,
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
	0xD014, 0x55, /*new setting*/
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
	0xD097, 0x98, /*new setting end*/

	0x5869, 0x01,

	/* IQ quality */
	0x68A9, 0x00,
	0x68C5, 0x00,
	0x68DF, 0x00,
	0x6953, 0x01,
	0x6962, 0x3A,
	0x69CD, 0x3A,
	0x9258, 0x00,
	0x9906, 0x00,
	0x9907, 0x28,
	0x9976, 0x0A,
	0x99B0, 0x20,
	0x99B1, 0x20,
	0x99B2, 0x20,
	0x99C6, 0x6E,
	0x99C7, 0x6E,
	0x99C8, 0x6E,
	0x9A1F, 0x0A,
	0x9AB0, 0x20,
	0x9AB1, 0x20,
	0x9AB2, 0x20,
	0x9AC6, 0x6E,
	0x9AC7, 0x6E,
	0x9AC8, 0x6E,
	0x9B01, 0x08,
	0x9B03, 0x1B,
	0x9B05, 0x20,
	0x9B07, 0x28,
	0x9B08, 0x01,
	0x9B09, 0x33,
	0x9B0A, 0x01,
	0x9B0B, 0x40,
	0x9B13, 0x10,
	0x9B15, 0x1D,
	0x9B17, 0x20,
	0x9B25, 0x60,
	0x9B27, 0x60,
	0x9B29, 0x60,
	0x9B2B, 0x40,
	0x9B2D, 0x40,
	0x9B2F, 0x40,
	0x9B37, 0x80,
	0x9B39, 0x80,
	0x9B3B, 0x80,
	0x9B5D, 0x08,
	0x9B5E, 0x0E,
	0x9B60, 0x08,
	0x9B61, 0x0E,
	0x9B76, 0x0A,
	0x9BB0, 0x20,
	0x9BB1, 0x20,
	0x9BB2, 0x20,
	0x9BC6, 0x6E,
	0x9BC7, 0x6E,
	0x9BC8, 0x6E,
	0x9BCC, 0x20,
	0x9BCD, 0x20,
	0x9BCE, 0x20,
	0x9C01, 0x10,
	0x9C03, 0x1D,
	0x9C05, 0x20,
	0x9C13, 0x10,
	0x9C15, 0x10,
	0x9C17, 0x10,
	0x9C19, 0x04,
	0x9C1B, 0x67,
	0x9C1D, 0x80,
	0x9C1F, 0x0A,
	0x9C21, 0x29,
	0x9C23, 0x32,
	0x9C27, 0x56,
	0x9C29, 0x60,
	0x9C39, 0x67,
	0x9C3B, 0x80,
	0x9C3D, 0x80,
	0x9C3F, 0x80,
	0x9C41, 0x80,
	0x9C55, 0xC8,
	0x9C57, 0xC8,
	0x9C59, 0xC8,
	0x9C87, 0x48,
	0x9C89, 0x48,
	0x9C8B, 0x48,
	0x9CB0, 0x20,
	0x9CB1, 0x20,
	0x9CB2, 0x20,
	0x9CC6, 0x6E,
	0x9CC7, 0x6E,
	0x9CC8, 0x6E,
	0x9D13, 0x10,
	0x9D15, 0x10,
	0x9D17, 0x10,
	0x9D19, 0x04,
	0x9D1B, 0x67,
	0x9D1F, 0x0A,
	0x9D21, 0x29,
	0x9D23, 0x32,
	0x9D55, 0xC8,
	0x9D57, 0xC8,
	0x9D59, 0xC8,
	0x9D91, 0x20,
	0x9D93, 0x20,
	0x9D95, 0x20,
	0x9E01, 0x10,
	0x9E03, 0x1D,
	0x9E13, 0x10,
	0x9E15, 0x10,
	0x9E17, 0x10,
	0x9E19, 0x04,
	0x9E1B, 0x67,
	0x9E1D, 0x80,
	0x9E1F, 0x0A,
	0x9E21, 0x29,
	0x9E23, 0x32,
	0x9E25, 0x30,
	0x9E27, 0x56,
	0x9E29, 0x60,
	0x9E39, 0x67,
	0x9E3B, 0x80,
	0x9E3D, 0x80,
	0x9E3F, 0x80,
	0x9E41, 0x80,
	0x9E55, 0xC8,
	0x9E57, 0xC8,
	0x9E59, 0xC8,
	0x9E91, 0x20,
	0x9E93, 0x20,
	0x9E95, 0x20,
	0x9F8F, 0xA0,
	0xA027, 0x67,
	0xA029, 0x80,
	0xA02D, 0x67,
	0xA02F, 0x80,
	0xA031, 0x80,
	0xA033, 0x80,
	0xA035, 0x80,
	0xA037, 0x80,
	0xA039, 0x80,
	0xA03B, 0x80,
	0xA067, 0x20,
	0xA068, 0x20,
	0xA069, 0x20,
	0xA071, 0x48,
	0xA073, 0x48,
	0xA075, 0x48,
	0xA08F, 0xA0,
	0xA091, 0x3A,
	0xA093, 0x3A,
	0xA095, 0x0A,
	0xA097, 0x0A,
	0xA099, 0x0A,

	0x9012, 0x00,
	0x9098, 0x1A,
	0x9099, 0x04,
	0x909A, 0x20,
	0x909B, 0x20,
	0x909C, 0x13,
	0x909D, 0x13,
	0xA716, 0x13,
	0xA801, 0x08,
	0xA803, 0x0C,
	0xA805, 0x10,
	0xA806, 0x00,
	0xA807, 0x18,
	0xA808, 0x00,
	0xA809, 0x20,
	0xA80A, 0x00,
	0xA80B, 0x30,
	0xA80C, 0x00,
	0xA80D, 0x40,
	0xA80E, 0x00,
	0xA80F, 0x60,
	0xA810, 0x00,
	0xA811, 0x80,
	0xA812, 0x00,
	0xA813, 0xC0,
	0xA814, 0x01,
	0xA815, 0x00,
	0xA816, 0x01,
	0xA817, 0x80,
	0xA818, 0x02,
	0xA819, 0x00,
	0xA81A, 0x03,
	0xA81B, 0x00,
	0xA81C, 0x03,
	0xA81D, 0xAC,
	0xA838, 0x03,
	0xA83C, 0x28,
	0xA83D, 0x5F,
	0xA881, 0x08,
	0xA883, 0x0C,
	0xA885, 0x10,
	0xA886, 0x00,
	0xA887, 0x18,
	0xA888, 0x00,
	0xA889, 0x20,
	0xA88A, 0x00,
	0xA88B, 0x30,
	0xA88C, 0x00,
	0xA88D, 0x40,
	0xA88E, 0x00,
	0xA88F, 0x60,
	0xA890, 0x00,
	0xA891, 0x80,
	0xA892, 0x00,
	0xA893, 0xC0,
	0xA894, 0x01,
	0xA895, 0x00,
	0xA896, 0x01,
	0xA897, 0x80,
	0xA898, 0x02,
	0xA899, 0x00,
	0xA89A, 0x03,
	0xA89B, 0x00,
	0xA89C, 0x03,
	0xA89D, 0xAC,
	0xA8B8, 0x03,
	0xA8BB, 0x13,
	0xA8BC, 0x28,
	0xA8BD, 0x25,
	0xA8BE, 0x1D,
	0xA8C0, 0x3A,
	0xA8C1, 0xE0,
	0xB24F, 0x80,

	0x3198, 0x0F,
	0x31A0, 0x04,
	0x31A1, 0x03,
	0x31A2, 0x02,
	0x31A3, 0x01,
	0x31A8, 0x18,
	0x822C, 0x01,
	0x9503, 0x07,
	0x9504, 0x07,
	0x9505, 0x07,
	0x9506, 0x00,
	0x9507, 0x00,
	0x9508, 0x00,
	0x9526, 0x18,
	0x9527, 0x18,
	0x9528, 0x18,

	0x8858, 0x00,

	0x6B42, 0x40,
	0x6B46, 0x00,
	0x6B47, 0x4B,
	0x6B4A, 0x00,
	0x6B4B, 0x4B,
	0x6B4E, 0x00,
	0x6B4F, 0x4B,
	0x6B44, 0x00,
	0x6B45, 0x8C,
	0x6B48, 0x00,
	0x6B49, 0x8C,
	0x6B4C, 0x00,
	0x6B4D, 0x8C,
	0x0138, 0x01,
};
static void sensor_init(void)
{
	pr_debug("E\n");

	imx230_table_write_cmos_sensor(addr_data_pair_init_imx230,
		sizeof(addr_data_pair_init_imx230)/sizeof(kal_uint16));
} /*    sensor_init  */

kal_uint16 addr_data_pair_preview_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x08,
	0x0341, 0x3C,
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
	0x0902, 0x00,
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
	0x0307, 0x7F,
	0x0309, 0x0A,
	0x030B, 0x02,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xEB,
	0x0310, 0x01,

	0x0820, 0x07,
	0x0821, 0x58,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x08,
	0x0203, 0x32,
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

	0x3A22, 0x20,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x07,
	0x3A26, 0xD8,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x00,
	0x3A32, 0x00,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0F,
	0x3A36, 0xAF,
	0x3A37, 0x00,
	0x3A38, 0x01,
	0x3A39, 0x00,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
};


static void preview_setting(void)
{
	imx230_table_write_cmos_sensor(addr_data_pair_preview_imx230,
		sizeof(addr_data_pair_preview_imx230)/sizeof(kal_uint16));

	if (imgsensor.pdaf_mode == 1) {
		pr_info("GC pre PDAF\n");
		/*PDAF*/
		/*PD_CAL_ENALBE */
		write_cmos_sensor(0x3121, 0x01);
		/*AREA MODE */
		write_cmos_sensor(0x31B0, 0x01);	/* 8x6 output */
		/*PD_OUT_EN=1 */
		write_cmos_sensor(0x3123, 0x01);

		/*Fixed area mode */
		write_cmos_sensor(0x3150, 0x00);
		write_cmos_sensor(0x3151, 0x00);
		write_cmos_sensor(0x3152, 0x00);
		write_cmos_sensor(0x3153, 0x00);
		write_cmos_sensor(0x3154, 0x0A);
		write_cmos_sensor(0x3155, 0x70);
		write_cmos_sensor(0x3156, 0x07);
		write_cmos_sensor(0x3157, 0xD8);
		imx230_apply_SPC();
	}
}				/*    preview_setting  */

kal_uint16 addr_data_pair_preview_hdr_es2_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x03,
	0x0221, 0x22,
	0x0222, 0x02,	/* 0x10,modify to 2x ratio */
	0x0340, 0x08,
	0x0341, 0x3C,
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
	0x0902, 0x00,
	0x3000, 0x75,
	0x3001, 0x01,	/*bit[0]HDR enable */
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
	0x0307, 0x7F,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0C,
	0x030E, 0x01,
	0x030F, 0xD6,
	0x0310, 0x01,

	0x0820, 0x0e,
	0x0821, 0xb0,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x08,
	0x0203, 0x32,
	0x0224, 0x00,
	0x0225, 0xb3,

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
	0x3007, 0x01,
	0x31E0, 0x3f,
	0x31E1, 0xFF,
	0x31E4, 0x00,

	0x3A22, 0x00,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x0f,
	0x3A26, 0xb0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x00,
	0x3A32, 0x00,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0F,
	0x3A36, 0xAF,
	0x3A37, 0x00,
	0x3A38, 0x00,
	0x3A39, 0x10,

	0x3A21, 0x02,

	0x3011, 0x00,
	0x3013, 0x01,
};

static void preview_setting_HDR_ES2(void)
{

	imx230_table_write_cmos_sensor(addr_data_pair_preview_hdr_es2_imx230,
	   sizeof(addr_data_pair_preview_hdr_es2_imx230)/sizeof(kal_uint16));

	if (imgsensor.pdaf_mode == 1) {
		/*bit[0]PDAF enable during HDR on */
		write_cmos_sensor(0x3001, 0x01);
		/*PDAF*/
		/*PD_CAL_ENALBE */
		write_cmos_sensor(0x3121, 0x01);
		/*AREA MODE */
		write_cmos_sensor(0x31B0, 0x01);	/* 8x6 output */
		/*PD_OUT_EN=1 */
		write_cmos_sensor(0x3123, 0x01);

		/*Fixed area mode */
		write_cmos_sensor(0x3150, 0x00);
		write_cmos_sensor(0x3151, 0x00);
		write_cmos_sensor(0x3152, 0x00);
		write_cmos_sensor(0x3153, 0x00);
		write_cmos_sensor(0x3154, 0x0A);
		write_cmos_sensor(0x3155, 0x70);
		write_cmos_sensor(0x3156, 0x07);
		write_cmos_sensor(0x3157, 0xD8);
		imx230_apply_SPC();
	}


}				/*    preview_setting  */

kal_uint16 addr_data_pair_cap_15fps_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x10,
	0x0341, 0x1E,
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
	0x0307, 0x7D,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0C,
	0x030E, 0x01,
	0x030F, 0xB8,
	0x0310, 0x01,

	0x0820, 0x0D,
	0x0821, 0xC0,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x10,
	0x0203, 0x14,
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

	0x3A22, 0x20,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x0F,
	0x3A26, 0xB0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x00,
	0x3A32, 0x00,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0F,
	0x3A36, 0xAF,
	0x3A37, 0x00,
	0x3A38, 0x00,
	0x3A39, 0x00,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
};

kal_uint16 addr_data_pair_cap_24fps_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x10,
	0x0341, 0x20,
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
	0x0307, 0xC7,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0F,
	0x030E, 0x03,
	0x030F, 0x6E,
	0x0310, 0x01,

	0x0820, 0x15,
	0x0821, 0xF3,
	0x0822, 0x33,
	0x0823, 0x33,

	0x0202, 0x10,
	0x0203, 0x16,
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

	0x3A22, 0x20,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x0F,
	0x3A26, 0xB0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x00,
	0x3A32, 0x00,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0F,
	0x3A36, 0xAF,
	0x3A37, 0x00,
	0x3A38, 0x00,
	0x3A39, 0x00,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,

};

static void capture_setting(kal_uint16 currefps)
{
	pr_info("E! currefps:%d\n", currefps);
	if (currefps == 150) {	/* 15fps for PIP */

		imx230_table_write_cmos_sensor(addr_data_pair_cap_15fps_imx230,
		   sizeof(addr_data_pair_cap_15fps_imx230)/sizeof(kal_uint16));

	} else { /* 24fps            //2fps for Normal capture & ZSD */
		imx230_table_write_cmos_sensor(addr_data_pair_cap_24fps_imx230,
		   sizeof(addr_data_pair_cap_24fps_imx230)/sizeof(kal_uint16));
	}
}

kal_uint16 addr_data_pair_cap_es2_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x03,
	0x0221, 0x11,
	0x0222, 0x02,	/* 0x10,modify to 2x ratio */
	0x0340, 0x10,
	0x0341, 0x1E,
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
	0x3001, 0x01,	/*bit[0]HDR enable */
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
	0x0307, 0xC7,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0C,
	0x030E, 0x02,
	0x030F, 0xC1,
	0x0310, 0x01,

	0x0820, 0x16,
	0x0821, 0x08,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x10,
	0x0203, 0x14,
	0x0224, 0x02,
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
	0x3007, 0x01,
	0x31E0, 0x3F,
	0x31E1, 0xFF,
	0x31E4, 0x00,

	0x3A22, 0x00,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x0F,
	0x3A26, 0xB0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x00,
	0x3A32, 0x00,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0F,
	0x3A36, 0xAF,
	0x3A37, 0x00,
	0x3A38, 0x00,
	0x3A39, 0x00,

	0x3A21, 0x02,

	0x3011, 0x00,
	0x3013, 0x01,

};

static void capture_setting_HDR_ES2(void)
{
	imx230_table_write_cmos_sensor(addr_data_pair_cap_es2_imx230,
		sizeof(addr_data_pair_cap_es2_imx230)/sizeof(kal_uint16));


	/*24fps for Normal capture & ZSD */



	/*PDAF*/
	write_cmos_sensor(0x3001, 0x01);
	/*PD_CAL_ENALBE */
	write_cmos_sensor(0x3121, 0x01);
	/*AREA MODE */
	write_cmos_sensor(0x31B0, 0x01);
	/*PD_OUT_EN=1 */
	write_cmos_sensor(0x3123, 0x01);


	/*Fixed area mode */
	write_cmos_sensor(0x3150, 0x00);
	write_cmos_sensor(0x3151, 0x00);
	write_cmos_sensor(0x3152, 0x00);
	write_cmos_sensor(0x3153, 0x00);
	write_cmos_sensor(0x3154, 0x14);
	write_cmos_sensor(0x3155, 0xE0);
	write_cmos_sensor(0x3156, 0x0F);
	write_cmos_sensor(0x3157, 0xB0);

}

kal_uint16 addr_data_pair_cap_pdaf_15fps_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x10,
	0x0341, 0x1E,
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
	0x0307, 0x7D,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0C,
	0x030E, 0x01,
	0x030F, 0xB8,
	0x0310, 0x01,

	0x0820, 0x0D,
	0x0821, 0xC0,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x10,
	0x0203, 0x14,
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

	0x3A22, 0x20,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x0F,
	0x3A26, 0xB0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x00,
	0x3A32, 0x00,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0F,
	0x3A36, 0xAF,
	0x3A37, 0x00,
	0x3A38, 0x00,
	0x3A39, 0x00,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
};

kal_uint16 addr_data_pair_cap_pdaf_24fps_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x10,
	0x0341, 0x1E,
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
	0x0307, 0xC7,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0C,
	0x030E, 0x02,
	0x030F, 0xC1,
	0x0310, 0x01,

	0x0820, 0x16,
	0x0821, 0x08,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x10,
	0x0203, 0x14,
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

	0x3A22, 0x20,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x0F,
	0x3A26, 0xB0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x00,
	0x3A32, 0x00,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0F,
	0x3A36, 0xAF,
	0x3A37, 0x00,
	0x3A38, 0x00,
	0x3A39, 0x00,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
};
static void capture_setting_pdaf(kal_uint16 currefps)
{
	pr_info("E! currefps:%d\n", currefps);
	if (currefps == 150) {	/* 15fps for PIP */

		imx230_table_write_cmos_sensor(
			addr_data_pair_cap_pdaf_15fps_imx230,
	   sizeof(addr_data_pair_cap_pdaf_15fps_imx230)/sizeof(kal_uint16));


	} else { /* 24fps            //24fps for Normal capture & ZSD */
		imx230_table_write_cmos_sensor(
			addr_data_pair_cap_pdaf_24fps_imx230,
	   sizeof(addr_data_pair_cap_pdaf_24fps_imx230)/sizeof(kal_uint16));

	}
	/*PDAF*/
	/*PD_CAL_ENALBE */
	write_cmos_sensor(0x3121, 0x01);
	/*AREA MODE */
	write_cmos_sensor(0x31B0, 0x01);	/* 8x6 output */
	/*PD_OUT_EN=1 */
	write_cmos_sensor(0x3123, 0x01);

	/*Fixed area mode */
	write_cmos_sensor(0x3150, 0x00);
	write_cmos_sensor(0x3151, 0x00);
	write_cmos_sensor(0x3152, 0x00);
	write_cmos_sensor(0x3153, 0x00);
	write_cmos_sensor(0x3154, 0x14);
	write_cmos_sensor(0x3155, 0xE0);
	write_cmos_sensor(0x3156, 0x0F);
	write_cmos_sensor(0x3157, 0xB0);
	imx230_apply_SPC();

}

kal_uint16 addr_data_pair_video_imx230[] = {

	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x0C,
	0x0341, 0x20,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0xFA,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0D,
	0x034B, 0xB5,
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
	0x034F, 0xBC,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x02,
	0x040C, 0x14,
	0x040D, 0xE0,
	0x040E, 0x0B,
	0x040F, 0xBC,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xBB,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x01,
	0x030F, 0x4A,
	0x0310, 0x01,

	0x0820, 0x14,
	0x0821, 0xA0,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x0C,
	0x0203, 0x16,
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

	0x3A22, 0x20,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x0B,
	0x3A26, 0xBC,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x01,
	0x3A32, 0xFA,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0D,
	0x3A36, 0xB5,
	0x3A37, 0x00,
	0x3A38, 0x00,
	0x3A39, 0x00,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
};

static void normal_video_setting(kal_uint16 currefps)
{

	pr_info("E! currefps:%d\n", currefps);
	imx230_table_write_cmos_sensor(addr_data_pair_video_imx230,
		sizeof(addr_data_pair_video_imx230)/sizeof(kal_uint16));

}
kal_uint16 addr_data_pair_video_hdr_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x03,
	0x0221, 0x11,
	0x0222, 0x02,	/* 0x10,modify to 2x ratio */
	0x0340, 0x0C,
	0x0341, 0x30,
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
	0x034F, 0xBE,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x02,
	0x040C, 0x14,
	0x040D, 0xE0,
	0x040E, 0x0B,
	0x040F, 0xBE,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xBC,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x0C,
	0x030E, 0x02,
	0x030F, 0x99,
	0x0310, 0x01,

	0x0820, 0x14,
	0x0821, 0xC8,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x0C,
	0x0203, 0x26,
	0x0224, 0x0C,
	0x0225, 0x26,

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
	0x3007, 0x01,
	0x31E0, 0x3F,
	0x31E1, 0xFF,
	0x31E4, 0x02,

	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x0B,
	0x3A26, 0xC0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x01,
	0x3A32, 0xF8,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0D,
	0x3A36, 0xB7,
	0x3A37, 0x00,
	0x3A38, 0x00,
	0x3A39, 0x00,

	0x3A21, 0x02,

	0x3011, 0x00,
	0x3013, 0x01,
};

static void normal_video_setting_30fps_HDR(void)
{
	pr_info("E\n");
	imx230_table_write_cmos_sensor(addr_data_pair_video_hdr_imx230,
		sizeof(addr_data_pair_video_hdr_imx230)/sizeof(kal_uint16));
}
kal_uint16 addr_data_pair_hs_video_imx230[] = {
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
	0x0347, 0x38,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0D,
	0x034B, 0x77,
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
	0x034D, 0x00,
	0x034E, 0x02,
	0x034F, 0xD0,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x1C,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x05,
	0x040D, 0x00,
	0x040E, 0x02,
	0x040F, 0xD0,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x02,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xC6,
	0x0310, 0x01,

	0x0820, 0x06,
	0x0821, 0x30,
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

	0x3A22, 0x20,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x02,
	0x3A26, 0xD0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x02,
	0x3A32, 0x38,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0D,
	0x3A36, 0x77,
	0x3A37, 0x00,
	0x3A38, 0x02,
	0x3A39, 0x00,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
};

static void hs_video_setting(void)
{
	pr_info("E\n");
	imx230_table_write_cmos_sensor(addr_data_pair_hs_video_imx230,
		sizeof(addr_data_pair_hs_video_imx230)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_slim_video_imx230[] = {
	0x0114, 0x03,
	0x0220, 0x00,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0340, 0x0C,
	0x0341, 0xF8,
	0x0342, 0x17,
	0x0343, 0x88,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0x38,
	0x0348, 0x14,
	0x0349, 0xDF,
	0x034A, 0x0D,
	0x034B, 0x77,
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
	0x034D, 0x00,
	0x034E, 0x02,
	0x034F, 0xD0,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x1C,
	0x040A, 0x00,
	0x040B, 0x10,
	0x040C, 0x05,
	0x040D, 0x00,
	0x040E, 0x02,
	0x040F, 0xD0,

	0x0301, 0x04,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x02,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xC6,
	0x0310, 0x01,

	0x0820, 0x06,
	0x0821, 0x30,
	0x0822, 0x00,
	0x0823, 0x00,

	0x0202, 0x0C,
	0x0203, 0xEE,
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

	0x3A22, 0x20,
	0x3A23, 0x14,
	0x3A24, 0xE0,
	0x3A25, 0x02,
	0x3A26, 0xF0,
	0x3A2F, 0x00,
	0x3A30, 0x00,
	0x3A31, 0x02,
	0x3A32, 0x38,
	0x3A33, 0x14,
	0x3A34, 0xDF,
	0x3A35, 0x0D,
	0x3A36, 0x77,
	0x3A37, 0x00,
	0x3A38, 0x02,
	0x3A39, 0x00,

	0x3A21, 0x00,

	0x3011, 0x00,
	0x3013, 0x01,
};

static void slim_video_setting(void)
{
	pr_info("E\n");
	/* @@video_720p_30fps_800Mbps */
	imx230_table_write_cmos_sensor(addr_data_pair_slim_video_imx230,
		sizeof(addr_data_pair_slim_video_imx230)/sizeof(kal_uint16));
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_info("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0601, 0x02);
	else
		write_cmos_sensor(0x0601, 0x00);

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
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
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
			pr_info("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct,
		 * Must set *sensor_id to 0xFFFFFFFF
		 */
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

	pr_info("IMX230,MIPI 4LANE\n");
	pr_info(
	"preview 2672*2008@30fps; video 5344*4016@30fps; capture 21M@24fps\n");
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
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
			pr_info("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
	read_imx230_SPC(imx230_SPC_data);

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
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
	pr_info("E\n");

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
	pr_info("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (imgsensor.ihdr_mode == 2)
		preview_setting_HDR_ES2();	/*HDR + PDAF */
	else
		preview_setting();	/*PDAF only */
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
	pr_info("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			pr_info(
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
	if (imgsensor.ihdr_mode == 2)
		capture_setting_HDR_ES2();	/*HDR+PDAF */
	else if (imgsensor.pdaf_mode == 1)
		capture_setting_pdaf(imgsensor.current_fps);	/*PDAF only */
	else
		capture_setting(imgsensor.current_fps);	/*Full mode */

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (imgsensor.ihdr_mode == 2)
		normal_video_setting_30fps_HDR();
	else
		normal_video_setting(imgsensor.current_fps);
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("E\n");

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
	pr_info("E\n");

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

	return ERROR_NONE;
}				/*    slim_video     */



static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
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
}				/*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/*pr_debug("scenario_id = %d\n", scenario_id);*/

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

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	/* The frame of setting sensor gain */

	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode */
	sensor_info->PDAF_Support = 2;

	sensor_info->HDR_Support = 2;	/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR */

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
	sensor_info->TEMPERATURE_SUPPORT = 1;

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
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{				/* This Function not used after ROME */
	pr_info("framerate = %d\n ", framerate);
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

static kal_uint32 set_auto_flicker_mode(
					kal_bool enable, UINT16 framerate)
{
	pr_info("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_info("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

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
		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10 /
		    imgsensor_info.normal_video.linelength;

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
			frame_length =
			    imgsensor_info.cap1.pclk / framerate * 10 /
			    imgsensor_info.cap1.linelength;
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
				pr_info(
				"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);

			frame_length =
			    imgsensor_info.cap.pclk / framerate * 10 /
			    imgsensor_info.cap.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;

			imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
	break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 /
		    imgsensor_info.hs_video.linelength;

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
		frame_length =
		    imgsensor_info.slim_video.pclk / framerate * 10 /
		    imgsensor_info.slim_video.linelength;
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
	default:		/* coding with  preview scenario by default */
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 /
						imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		pr_info("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	/*pr_debug("scenario_id = %d\n", scenario_id);*/

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


static kal_uint32 imx230_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	UINT32 rgain_32 = 0;
	UINT32 grgain_32 = 0;
	UINT32 gbgain_32 = 0;
	UINT32 bgain_32 = 0;

	pr_info("imx230_awb_gain\n");

	grgain_32 = (pSetSensorAWB->ABS_GAIN_GR << 8) >> 9;
	rgain_32 = (pSetSensorAWB->ABS_GAIN_R << 8) >> 9;
	bgain_32 = (pSetSensorAWB->ABS_GAIN_B << 8) >> 9;
	gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB << 8) >> 9;

	pr_info("[imx230_awb_gain] ABS_GAIN_GR:%d, grgain_32:%d\n",
		pSetSensorAWB->ABS_GAIN_GR,
		grgain_32);
	pr_info("[imx230_awb_gain] ABS_GAIN_R:%d, rgain_32:%d\n",
		pSetSensorAWB->ABS_GAIN_R,
		rgain_32);
	pr_info("[imx230_awb_gain] ABS_GAIN_B:%d, bgain_32:%d\n",
		pSetSensorAWB->ABS_GAIN_B,
		bgain_32);
	pr_info("[imx230_awb_gain] ABS_GAIN_GB:%d, gbgain_32:%d\n",
		pSetSensorAWB->ABS_GAIN_GB,
		gbgain_32);

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

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_info("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor(0x0100, 0X01);
	else
		write_cmos_sensor(0x0100, 0x00);
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

	/*pr_info("temp_c(%d), read_reg(%d)\n",
	 *	temperature_convert, temperature);
	 */

	return temperature_convert;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;

/* unsigned long long *feature_return_data =
 * (unsigned long long*)feature_para;
 */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;

	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB =
		(struct SET_SENSOR_AWB_GAIN *)feature_para;

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
		set_shutter((kal_uint32) (*feature_data));
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
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */
		/* if EEPROM does not exist in camera module. */
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
			(BOOL) (*feature_data_16), *(feature_data_16 + 1));
		break;

	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
					      *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;

	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_info("SENSOR_FEATURE_GET_PDAF_DATA\n");
		read_imx230_DCC((kal_uint16) (*feature_data),
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
		pr_info("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);
		wininfo =
	 (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t) (*(feature_data + 1));

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
		/*HDR CMD */
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("hdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_info("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

		ihdr_write_shutter_gain(*feature_data,
			*(feature_data + 1), *(feature_data + 2));
		break;

	case SENSOR_FEATURE_GET_VC_INFO:
		pr_info("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);
		pvcinfo =
	     (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t) (*(feature_data + 1));
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
		imx230_awb_gain(pSetSensorAWB);
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		pr_info("SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu\n",
			*feature_data);

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x2;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x2;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x2;
			break;
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		}
		break;

		/*END OF HDR CMD */
		/*PDAF CMD */
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_info("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n",
			*feature_data);
	/* PDAF capacity enable or not, 2p8 only full size support PDAF */
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
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		pr_info("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));

		imx230_get_pdaf_reg_setting(
			(*feature_para_len) / sizeof(UINT32), feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		pr_info("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));

		imx230_set_pdaf_reg_setting(
			(*feature_para_len) / sizeof(UINT32), feature_data_16);
		break;

	case SENSOR_FEATURE_SET_PDAF:
		pr_info("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
		/*End of PDAF */
	case SENSOR_FEATURE_SET_PDFOCUS_AREA:
		pr_info(
		"SENSOR_FEATURE_SET_IMX230_PDFOCUS_AREA Start Pos=%d, Size=%d\n",
		(UINT32) *feature_data, (UINT32) *(feature_data + 1));

		//imx230_set_pd_focus_area(*feature_data, *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_info("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
		hdr_write_shutter(
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_info("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_info(
		    "SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
		    *feature_data);

		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
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

UINT32 IMX230_MIPI_RAW_SensorInit(
			struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    IMX230_MIPI_RAW_SensorInit    */
