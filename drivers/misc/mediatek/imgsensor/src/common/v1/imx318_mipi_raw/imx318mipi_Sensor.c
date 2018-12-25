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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     IMX318mipi_Sensor.c
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

/******************Modify Following Strings for Debug**************************/
#define PFX "IMX318_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__
/****************************   Modify end    *********************************/

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

#include "imx318mipi_Sensor.h"








#define BYTE               unsigned char

static BOOL read_spc_flag = FALSE;

#define HIGH_SPEED_240FPS

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static BYTE imx318_SPC_data[352] = { 0 };

/* extern void read_imx318_SPC( BYTE* data ); */
/* extern void read_imx318_DCC( kal_uint16 addr,BYTE* data, kal_uint32 size); */



static struct imgsensor_info_struct imgsensor_info = {

	/* record sensor id defined in Kd_imgsensor.h */
	.sensor_id = IMX318_SENSOR_ID,

	.checksum_value = 0xafd83a68, /* checksum value for Camera Auto Test */

	.pre = {
		.pclk = 480000000,	/* record different mode's pclk */
		.linelength = 7352,	/* record different mode's linelength */
		.framelength = 2176, /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2744,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 2056,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 799200000,
		.linelength = 6224,
		.framelength = 4280,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5488,
		.grabwindow_height = 4112,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 300,
		},
	/*
	 * capture for PIP 24fps relative information,
	 * capture1 mode must use same framelength,
	 * linelength with Capture mode for shutter calculate
	 */
	.cap1 = {
		 .pclk = 799200000,
		 .linelength = 6224,
		 .framelength = 4280,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 5488,
		 .grabwindow_height = 4112,
		 .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		 .max_framerate = 240,
		 /* less than 13M(include 13M),cap1 max framerate is 24fps,
		  * 16M max framerate is 20fps, 20M max framerate is 15fps
		  */
		 },
	.normal_video = {
			 .pclk = 693600000,
			 .linelength = 7352,
			 .framelength = 3136,
			 .startx = 0,
			 .starty = 0,
			 .grabwindow_width = 3840,
			 .grabwindow_height = 2160,
			 .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
			 .max_framerate = 300,
			 },
#ifdef HIGH_SPEED_240FPS
	.hs_video = {
		     .pclk = 724800000,
		     .linelength = 3680,
		     .framelength = 820,
		     .startx = 0,
		     .starty = 0,
		     .grabwindow_width = 1372,
		     .grabwindow_height = 772,
		     .mipi_data_lp2hs_settle_dc = 85,
		     .max_framerate = 2400,
		     },
#else
	.hs_video = {
		     .pclk = 703200000,
		     .linelength = 3680,
		     .framelength = 1592,
		     .startx = 0,
		     .starty = 0,
		     .grabwindow_width = 2744,
		     .grabwindow_height = 1544,
		     .mipi_data_lp2hs_settle_dc = 85,
		     .max_framerate = 1200,
		     },
#endif
	.slim_video = {
		       .pclk = 201000000,
		       .linelength = 6024,
		       .framelength = 1112,
		       .startx = 0,
		       .starty = 0,
		       .grabwindow_width = 1280,
		       .grabwindow_height = 720,
		       .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		       .max_framerate = 300,
		       },
	.margin = 4,		/* sensor framelength & shutter margin */
	.min_shutter = 1,	/* min shutter */

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0x7fff,

	.ae_shut_delay_frame = 0,
	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */

	.ae_sensor_gain_delay_frame = 0,
	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 0,	/* 1, support; 0,not support */
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

	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	/* 0,MIPI_SETTLEDELAY_AUTO;
	 * 1,MIPI_SETTLEDELAY_MANNUAL
	 */

	/* sensor output first pixel color */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */

	.i2c_addr_table = {0x20, 0x34, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4must end with 0xff
	 */

	.i2c_speed = 400,	/* i2c read/write speed */
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */

	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */

	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */

	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 300,

	.autoflicker_en = KAL_FALSE,
	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */

	.test_pattern = KAL_FALSE,
	/* test pattern mode or not. KAL_FALSE for in test pattern mode,
	 * KAL_TRUE for normal output
	 */

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0,	 /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x6c,	/* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{5488, 4112, 0, 0, 5488, 4112, 2744, 2056,
	 0000, 0000, 2744, 2056, 0, 0, 2744, 2056},	/*preview */
	{5488, 4112, 0, 0, 5488, 4112, 5488, 4112,
	 0000, 0000, 5488, 4112, 0, 0, 5488, 4112},	/*capture */
	{5488, 4112, 0, 0, 5488, 4112, 5488, 4112,
	 824, 976, 3840, 2160, 0, 0, 3840, 2160},	/* video */
#ifdef HIGH_SPEED_240FPS
	{5488, 4112, 0, 516, 5488, 3088, 1372,
	  772, 0, 0, 1372, 772, 0, 0, 1372, 772},
#else
	{5488, 4112, 0, 512, 5488, 3088, 2744,
	 1544, 0, 0, 2744, 1544, 0, 0, 2744, 1544},
#endif
	{5488, 4112, 0, 0, 5488, 3006, 1280, 720,
	 0000, 0000, 1280, 720, 0, 0, 1280, 720}
};				/*slim video */

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
	 0x00, 0x2b, 0x14E0, 0x0FB0, 0x01, 0x00, 0x0000, 0x0000,
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

#define IMX318MIPI_MaxGainIndex (115)
kal_uint16 IMX318MIPI_sensorGainMapping[IMX318MIPI_MaxGainIndex][2] = {
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
static kal_uint32 imx318_ATR(UINT16 DarkLimit, UINT16 OverExp)
{
	/*
	 * write_cmos_sensor(0x6e50,sensorATR_Info[DarkLimit].DarkLimit_H);
	 * write_cmos_sensor(0x6e51,sensorATR_Info[DarkLimit].DarkLimit_L);
	 * write_cmos_sensor(0x9340,sensorATR_Info[OverExp].OverExp_Min_H);
	 * write_cmos_sensor(0x9341,sensorATR_Info[OverExp].OverExp_Min_L);
	 * write_cmos_sensor(0x9342,sensorATR_Info[OverExp].OverExp_Max_H);
	 * write_cmos_sensor(0x9343,sensorATR_Info[OverExp].OverExp_Max_L);
	 * write_cmos_sensor(0x9706,0x10);
	 * write_cmos_sensor(0x9707,0x03);
	 * write_cmos_sensor(0x9708,0x03);
	 * write_cmos_sensor(0x9e24,0x00);
	 * write_cmos_sensor(0x9e25,0x8c);
	 * write_cmos_sensor(0x9e26,0x00);
	 * write_cmos_sensor(0x9e27,0x94);
	 * write_cmos_sensor(0x9e28,0x00);
	 * write_cmos_sensor(0x9e29,0x96);
	 * pr_debug("DarkLimit 0x6e50(0x%x), 0x6e51(0x%x)\n",
	 * sensorATR_Info[DarkLimit].DarkLimit_H,
	 * sensorATR_Info[DarkLimit].DarkLimit_L);
	 * pr_debug("OverExpMin 0x9340(0x%x), 0x9341(0x%x)\n",
	 * sensorATR_Info[OverExp].OverExp_Min_H,
	 * sensorATR_Info[OverExp].OverExp_Min_L);
	 * pr_debug("OverExpMin 0x9342(0x%x), 0x9343(0x%x)\n",
	 * sensorATR_Info[OverExp].OverExp_Max_H,
	 * sensorATR_Info[OverExp].OverExp_Max_L);
	 */
	return ERROR_NONE;
}
#endif
static MUINT32 cur_startpos;
static MUINT32 cur_size;

static void imx318_set_pd_focus_area(MUINT32 startpos, MUINT32 size)
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
	"start_x:%d, start_y:%d, f_width:%d, f_height:%d, end_x:%d, end_y:%d\n",
start_x_pos, start_y_pos, focus_width, focus_height, end_x_pos, end_y_pos);

}

static void imx318_apply_SPC(void)
{
	unsigned int start_reg = 0x7c00;
	int i;

	if (read_spc_flag == FALSE) {
		/* read_imx318_SPC(imx318_SPC_data); */
		read_spc_flag = TRUE;
		return;
	}

	for (i = 0; i < 352; i++) {
		write_cmos_sensor(start_reg, imx318_SPC_data[i]);
		/* pr_debug("SPC[%d]= %x\n", i , imx318_SPC_data[i]); */

		start_reg++;
	}

}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
/*
 * you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel,
 * or you can set dummy by imgsensor.frame_length and imgsensor.line_length
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
	return ((read_cmos_sensor(0x0016) << 8) | read_cmos_sensor(0x0017));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	pr_debug("framerate = %d, min framelength should enable %d\n",
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
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* write_shutter(shutter); */
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK
	 * to get exposure larger than frame exposure
	 */
	/* AE doesn't update sensor gain at capture mode,
	 * thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length,
	 * should extend frame length first
	 */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter =
(shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		      write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
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

	pr_debug("Exit! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}				/*    set_shutter */



static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 iI;

	pr_debug("[IMX318MIPI]enter IMX318MIPIGain2Reg function\n");
	for (iI = 0; iI < IMX318MIPI_MaxGainIndex; iI++) {
		if (gain < IMX318MIPI_sensorGainMapping[iI][0])
			return IMX318MIPI_sensorGainMapping[iI][1];

	}
	if (iI != IMX318MIPI_MaxGainIndex) {
		if (gain != IMX318MIPI_sensorGainMapping[iI][0]) {
			pr_debug("Gain mapping don't correctly:%d %d\n", gain,
				IMX318MIPI_sensorGainMapping[iI][0]);
		}
	}
	return IMX318MIPI_sensorGainMapping[iI - 1][1];
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
	write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
	write_cmos_sensor(0x0205, reg_gain & 0xFF);
	write_cmos_sensor(0x0104, 0x00);

	return gain;
}				/*    set_gain  */

static void ihdr_write_shutter_gain(
				kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{

	kal_uint16 realtime_fps = 0;
	kal_uint16 reg_gain;

	pr_debug("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
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

#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3
#endif

static kal_uint16 imx318_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	/* Add this func to set i2c speed by each sensor */
	/* kdSetI2CSpeed(imgsensor_info.i2c_speed); */

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
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				  imgsensor.i2c_write_id,
				  3, imgsensor_info.i2c_speed);

			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

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
}				/*    night_mode    */


kal_uint16 addr_data_pair_init_imx318[] = {
	/*External Clock setting */
	0x0136, 0x18,
	0x0137, 0x00,
	/*Global setting */
	0x3067, 0x00,
	0x4600, 0x1B,
	0x46C2, 0x00,
	0x4877, 0x11,
	0x487B, 0x4D,
	0x487F, 0x3B,
	0x4883, 0xB4,
	0x4C6F, 0x5E,
	0x5113, 0xF4,
	0x5115, 0xF6,
	0x5125, 0xF4,
	0x5127, 0xF8,
	0x51CF, 0xF4,
	0x51E9, 0xF4,
	0x5483, 0x7A,
	0x5485, 0x7C,
	0x5495, 0x7A,
	0x5497, 0x7F,
	0x5515, 0xC3,
	0x5517, 0xC7,
	0x552B, 0x7A,
	0x5535, 0x7A,
	0x5A35, 0x1B,
	0x5C13, 0x00,
	0x5D89, 0xB1,
	0x5D8B, 0x2C,
	0x5D8D, 0x61,
	0x5D8F, 0xE1,
	0x5D91, 0x4D,
	0x5D93, 0xB4,
	0x5D95, 0x41,
	0x5D97, 0x96,
	0x5D99, 0x37,
	0x5D9B, 0x81,
	0x5D9D, 0x31,
	0x5D9F, 0x71,
	0x5DA1, 0x2B,
	0x5DA3, 0x64,
	0x5DA5, 0x27,
	0x5DA7, 0x5A,
	0x6009, 0x03,
	0x613A, 0x05,
	0x613C, 0x23,
	0x6142, 0x02,
	0x6143, 0x62,
	0x6144, 0x89,
	0x6145, 0x0A,
	0x6146, 0x24,
	0x6147, 0x28,
	0x6148, 0x90,
	0x6149, 0xA2,
	0x614A, 0x40,
	0x614B, 0x8A,
	0x614C, 0x01,
	0x614D, 0x12,
	0x614E, 0x2C,
	0x614F, 0x98,
	0x6150, 0xA2,
	0x615D, 0x37,
	0x615E, 0xE6,
	0x615F, 0x4B,
	0x616C, 0x41,
	0x616D, 0x05,
	0x616E, 0x48,
	0x616F, 0xC5,
	0x6174, 0xB9,
	0x6175, 0x42,
	0x6176, 0x44,
	0x6177, 0xC3,
	0x6178, 0x81,
	0x6179, 0x78,
	0x6182, 0x15,
	0x6A5F, 0x03,
	0x9302, 0xFF,

	/* IQ quality */
	0x7468, 0x03,
	0x7B65, 0x8C,
	0x7B67, 0x4B,
	0x7B69, 0x8C,
	0x7B6B, 0x4B,
	0x7B6D, 0x8C,
	0x7B6F, 0x4B,
	0x7B70, 0x40,
	0x9805, 0x04,
	0x9822, 0x03,
	0x9843, 0x01,
	0x9902, 0x00,
	0x9903, 0x01,
	0x9904, 0x00,
	0x9905, 0x01,
	0x990E, 0x00,
	0x9944, 0x3C,
	0x9947, 0x3C,
	0x994A, 0x8C,
	0x994B, 0x1B,
	0x994C, 0x0A,
	0x994D, 0x8C,
	0x994E, 0x1B,
	0x994F, 0x0A,
	0x9950, 0x8C,
	0x9951, 0x50,
	0x9952, 0x1B,
	0x9953, 0x8C,
	0x9954, 0x50,
	0x9955, 0x1B,
	0x996E, 0x50,
	0x996F, 0x3C,
	0x9970, 0x1B,
	0x9A08, 0x04,
	0x9A09, 0x05,
	0x9A0A, 0x04,
	0x9A0B, 0x05,
	0x9A0C, 0x04,
	0x9A0D, 0x05,
	0x9A0E, 0x06,
	0x9A0F, 0x07,
	0x9A10, 0x06,
	0x9A11, 0x07,
	0x9A12, 0x07,
	0x9A13, 0x07,
	0x9A14, 0x07,
	0x9A2B, 0x0F,
	0x9A2C, 0x0F,
	0x9A2D, 0x0F,
	0x9A2E, 0x0F,
	0x9A2F, 0x0F,
	0x9A36, 0x02,
	0x9A37, 0x02,
	0x9A3F, 0x0E,
	0x9A40, 0x0E,
	0x9A41, 0x0E,
	0x9A42, 0x0E,
	0x9A43, 0x0F,
	0x9A44, 0x0F,
	0x9A4C, 0x0F,
	0x9A4D, 0x0F,
	0x9A4E, 0x0F,
	0x9A4F, 0x0F,
	0x9A50, 0x0F,
	0x9A54, 0x0F,
	0x9A55, 0x0F,
	0x9A5C, 0x03,
	0x9A5E, 0x03,
	0x9A64, 0x0E,
	0x9A65, 0x0E,
	0x9A66, 0x0E,
	0x9A67, 0x0E,
	0x9A6F, 0x0F,
	0x9A70, 0x0F,
	0x9A71, 0x0F,
	0x9A72, 0x0F,
	0x9A73, 0x0F,
	0x9AAC, 0x06,
	0x9AAD, 0x06,
	0x9AAE, 0x06,
	0x9AAF, 0x06,
	0x9AB0, 0x06,
	0x9AB1, 0x06,
	0x9AB2, 0x06,
	0x9AB3, 0x07,
	0x9AB4, 0x07,
	0x9AB5, 0x07,
	0x9AB6, 0x07,
	0x9AB7, 0x07,
	0x9AB8, 0x06,
	0x9AB9, 0x06,
	0x9ABA, 0x06,
	0x9ABB, 0x06,
	0x9ABC, 0x06,
	0x9ABD, 0x07,
	0x9ABE, 0x07,
	0x9ABF, 0x07,
	0x9AC0, 0x07,
	0x9AC1, 0x07,
	0xA000, 0x00,
	0xA001, 0x00,
	0xA002, 0x00,
	0xA003, 0x00,
	0xA004, 0x00,
	0xA005, 0x00,
	0xA017, 0x10,
	0xA019, 0x10,
	0xA01B, 0x10,
	0xA01D, 0x35,
	0xA023, 0x31,
	0xA02F, 0x50,
	0xA041, 0x6B,
	0xA047, 0x40,
	0xA068, 0x00,
	0xA069, 0x00,
	0xA06A, 0x00,
	0xA06B, 0x00,
	0xA06C, 0x00,
	0xA06D, 0x00,
	0xA06E, 0x00,
	0xA06F, 0x00,
	0xA070, 0x00,
	0xA075, 0x50,
	0xA077, 0x50,
	0xA079, 0x50,
	0xA07B, 0x40,
	0xA07D, 0x40,
	0xA07F, 0x40,
	0xA0AD, 0x18,
	0xA0AE, 0x18,
	0xA0AF, 0x18,
	0xA0B6, 0x00,
	0xA0B7, 0x00,
	0xA0B8, 0x00,
	0xA0B9, 0x00,
	0xA0BA, 0x00,
	0xA0BB, 0x00,
	0xA0BD, 0x2D,
	0xA0C3, 0x2D,
	0xA0C9, 0x40,
	0xA0D5, 0x2F,
	0xA100, 0x00,
	0xA101, 0x00,
	0xA102, 0x00,
	0xA103, 0x00,
	0xA104, 0x00,
	0xA105, 0x00,
	0xA117, 0x10,
	0xA119, 0x10,
	0xA11B, 0x10,
	0xA11D, 0x35,
	0xA123, 0x31,
	0xA12F, 0x50,
	0xA13B, 0x35,
	0xA13D, 0x35,
	0xA13F, 0x35,
	0xA141, 0x6B,
	0xA147, 0x5A,
	0xA168, 0x3F,
	0xA169, 0x3F,
	0xA16A, 0x3F,
	0xA16B, 0x00,
	0xA16C, 0x00,
	0xA16D, 0x00,
	0xA16E, 0x3F,
	0xA16F, 0x3F,
	0xA170, 0x3F,
	0xA1B6, 0x00,
	0xA1B7, 0x00,
	0xA1B8, 0x00,
	0xA1B9, 0x00,
	0xA1BA, 0x00,
	0xA1BB, 0x00,
	0xA1BD, 0x42,
	0xA1C3, 0x42,
	0xA1C9, 0x5A,
	0xA1D5, 0x2F,
	0xA200, 0x00,
	0xA201, 0x00,
	0xA202, 0x00,
	0xA203, 0x00,
	0xA204, 0x00,
	0xA205, 0x00,
	0xA217, 0x10,
	0xA219, 0x10,
	0xA21B, 0x10,
	0xA21D, 0x35,
	0xA223, 0x31,
	0xA22F, 0x50,
	0xA241, 0x6B,
	0xA247, 0x40,
	0xA268, 0x00,
	0xA269, 0x00,
	0xA26A, 0x00,
	0xA26B, 0x00,
	0xA26C, 0x00,
	0xA26D, 0x00,
	0xA26E, 0x00,
	0xA26F, 0x00,
	0xA270, 0x00,
	0xA271, 0x00,
	0xA272, 0x00,
	0xA273, 0x00,
	0xA275, 0x50,
	0xA277, 0x50,
	0xA279, 0x50,
	0xA27B, 0x40,
	0xA27D, 0x40,
	0xA27F, 0x40,
	0xA2B6, 0x00,
	0xA2B7, 0x00,
	0xA2B8, 0x00,
	0xA2B9, 0x00,
	0xA2BA, 0x00,
	0xA2BB, 0x00,
	0xA2BD, 0x2D,
	0xA2D5, 0x2F,
	0xA300, 0x00,
	0xA301, 0x00,
	0xA302, 0x00,
	0xA303, 0x00,
	0xA304, 0x00,
	0xA305, 0x00,
	0xA317, 0x10,
	0xA319, 0x10,
	0xA31B, 0x10,
	0xA31D, 0x35,
	0xA323, 0x31,
	0xA32F, 0x50,
	0xA341, 0x6B,
	0xA347, 0x5A,
	0xA368, 0x0F,
	0xA369, 0x0F,
	0xA36A, 0x0F,
	0xA36B, 0x30,
	0xA36C, 0x00,
	0xA36D, 0x00,
	0xA36E, 0x3F,
	0xA36F, 0x3F,
	0xA370, 0x2F,
	0xA371, 0x30,
	0xA372, 0x00,
	0xA373, 0x00,
	0xA3B6, 0x00,
	0xA3B7, 0x00,
	0xA3B8, 0x00,
	0xA3B9, 0x00,
	0xA3BA, 0x00,
	0xA3BB, 0x00,
	0xA3BD, 0x42,
	0xA3D5, 0x2F,
	0xA400, 0x00,
	0xA401, 0x00,
	0xA402, 0x00,
	0xA403, 0x00,
	0xA404, 0x00,
	0xA405, 0x00,
	0xA407, 0x10,
	0xA409, 0x10,
	0xA40B, 0x10,
	0xA40D, 0x35,
	0xA413, 0x31,
	0xA41F, 0x50,
	0xA431, 0x6B,
	0xA437, 0x40,
	0xA454, 0x00,
	0xA455, 0x00,
	0xA456, 0x00,
	0xA457, 0x00,
	0xA458, 0x00,
	0xA459, 0x00,
	0xA45A, 0x00,
	0xA45B, 0x00,
	0xA45C, 0x00,
	0xA45D, 0x00,
	0xA45E, 0x00,
	0xA45F, 0x00,
	0xA48D, 0x18,
	0xA48E, 0x18,
	0xA48F, 0x18,
	0xA496, 0x00,
	0xA497, 0x00,
	0xA498, 0x00,
	0xA499, 0x00,
	0xA49A, 0x00,
	0xA49B, 0x00,
	0xA49D, 0x2D,
	0xA4A3, 0x2D,
	0xA4A9, 0x40,
	0xA4B5, 0x2F,
	0xA500, 0x00,
	0xA501, 0x00,
	0xA502, 0x00,
	0xA503, 0x00,
	0xA504, 0x00,
	0xA505, 0x00,
	0xA507, 0x10,
	0xA509, 0x10,
	0xA50B, 0x10,
	0xA50D, 0x35,
	0xA513, 0x31,
	0xA51F, 0x50,
	0xA52B, 0x35,
	0xA52D, 0x35,
	0xA52F, 0x35,
	0xA531, 0x6B,
	0xA537, 0x5A,
	0xA554, 0x3F,
	0xA555, 0x3F,
	0xA556, 0x3F,
	0xA557, 0x00,
	0xA558, 0x00,
	0xA559, 0x00,
	0xA55A, 0x3F,
	0xA55B, 0x3F,
	0xA55C, 0x3F,
	0xA55D, 0x00,
	0xA55E, 0x00,
	0xA55F, 0x00,
	0xA596, 0x00,
	0xA597, 0x00,
	0xA598, 0x00,
	0xA599, 0x00,
	0xA59A, 0x00,
	0xA59B, 0x00,
	0xA59D, 0x42,
	0xA5A3, 0x42,
	0xA5A9, 0x5A,
	0xA5B5, 0x2F,
	0xA653, 0x84,
	0xA65F, 0x00,
	0xA6B5, 0xFF,
	0xA6C1, 0x00,
	0xA74F, 0xA0,
	0xA753, 0xFE,
	0xA75D, 0x00,
	0xA75F, 0x00,
	0xA7B5, 0xFF,
	0xA7C1, 0x00,
	0xCA00, 0x01,
	0xCA12, 0x2C,
	0xCA13, 0x2C,
	0xCA14, 0x1C,
	0xCA15, 0x1C,
	0xCA16, 0x06,
	0xCA17, 0x06,
	0xCA1A, 0x0C,
	0xCA1B, 0x0C,
	0xCA1C, 0x06,
	0xCA1D, 0x06,
	0xCA1E, 0x00,
	0xCA1F, 0x00,
	0xCA21, 0x04,
	0xCA23, 0x04,
	0xCA2C, 0x00,
	0xCA2D, 0x10,
	0xCA2F, 0x10,
	0xCA30, 0x00,
	0xCA32, 0x10,
	0xCA35, 0x28,
	0xCA37, 0x80,
	0xCA39, 0x10,
	0xCA3B, 0x10,
	0xCA3C, 0x20,
	0xCA3D, 0x20,
	0xCA43, 0x02,
	0xCA45, 0x02,
	0xCA46, 0x01,
	0xCA47, 0x99,
	0xCA48, 0x01,
	0xCA49, 0x99,
	0xCA6C, 0x20,
	0xCA6D, 0x20,
	0xCA6E, 0x20,
	0xCA6F, 0x20,
	0xCA72, 0x00,
	0xCA73, 0x00,
	0xCA75, 0x04,
	0xCA77, 0x04,
	0xCA80, 0x00,
	0xCA81, 0x30,
	0xCA84, 0x00,
	0xCA86, 0x10,
	0xCA89, 0x28,
	0xCA8B, 0x80,
	0xCA8D, 0x10,
	0xCA8F, 0x10,
	0xCA90, 0x20,
	0xCA91, 0x20,
	0xCA97, 0x02,
	0xCA99, 0x02,
	0xCA9A, 0x01,
	0xCA9B, 0x99,
	0xCA9C, 0x01,
	0xCA9D, 0x99,
	0xCAAB, 0x28,
	0xCAAD, 0x39,
	0xCAAE, 0x53,
	0xCAAF, 0x67,
	0xCAB0, 0x45,
	0xCAB1, 0x47,
	0xCAB2, 0x01,
	0xCAB3, 0x6B,
	0xCAB4, 0x06,
	0xCAB5, 0x8C,
	0xCAB7, 0xA6,
	0xCAB8, 0x06,
	0xCAB9, 0x0A,
	0xCABA, 0x08,
	0xCABB, 0x05,
	0xCABC, 0x33,
	0xCABD, 0x73,
	0xCABE, 0x02,
	0xCABF, 0x17,
	0xCAC0, 0x28,
	0xCAC1, 0xC5,
	0xCAC2, 0x08,
	0xCAC4, 0x25,
	0xCAC5, 0x0A,
	0xCAC6, 0x00,
	0xCAC8, 0x15,
	0xCAC9, 0x78,
	0xCACA, 0x1E,
	0xCACF, 0x60,
	0xCAD1, 0x28,
	0xD01A, 0x00,
	0xD080, 0x0A,
	0xD081, 0x10,
	/*enable temperature sensor, TEMP_SEN_CTL: */
	/*0x0138, 0x01 */

};

static void sensor_init(void)
{
	pr_debug("E\n");

	imx318_table_write_cmos_sensor(addr_data_pair_init_imx318,
		sizeof(addr_data_pair_init_imx318) / sizeof(kal_uint16));

	pr_debug("L\n");
}				/*    sensor_init  */


static void preview_setting(void)
{
	pr_debug("E\n");

	write_cmos_sensor(0x0111, 0x02);
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0342, 0x1C);
	write_cmos_sensor(0x0343, 0xB8);
	write_cmos_sensor(0x0340, 0x08);
	write_cmos_sensor(0x0341, 0x80);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x15);
	write_cmos_sensor(0x0349, 0x6F);
	write_cmos_sensor(0x034A, 0x10);
	write_cmos_sensor(0x034B, 0x0F);
	write_cmos_sensor(0x31A2, 0x00);
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0221, 0x11);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x22);
	write_cmos_sensor(0x0902, 0x02);
	write_cmos_sensor(0x3010, 0x65);
	write_cmos_sensor(0x3011, 0x11);
	write_cmos_sensor(0x301C, 0x00);
	write_cmos_sensor(0x3045, 0x01);
	write_cmos_sensor(0x3194, 0x00);
	write_cmos_sensor(0x31A0, 0x00);
	write_cmos_sensor(0x31A1, 0x00);
	write_cmos_sensor(0xD5EC, 0x3A);
	write_cmos_sensor(0xD5ED, 0x00);
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x0A);
	write_cmos_sensor(0x040D, 0xB8);
	write_cmos_sensor(0x040E, 0x08);
	write_cmos_sensor(0x040F, 0x08);
	write_cmos_sensor(0x034C, 0x0A);
	write_cmos_sensor(0x034D, 0xB8);
	write_cmos_sensor(0x034E, 0x08);
	write_cmos_sensor(0x034F, 0x08);
	write_cmos_sensor(0x0301, 0x05);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0xC8);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x02);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x00);
	write_cmos_sensor(0x030F, 0xFA);
	write_cmos_sensor(0x0820, 0x0B);
	write_cmos_sensor(0x0821, 0xB8);
	write_cmos_sensor(0x0822, 0x00);
	write_cmos_sensor(0x0823, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3033, 0x01);
	write_cmos_sensor(0x3039, 0x00);
	write_cmos_sensor(0x303B, 0x00);
	write_cmos_sensor(0x306C, 0x00);
	write_cmos_sensor(0x306E, 0x0D);
	write_cmos_sensor(0x306F, 0x56);
	write_cmos_sensor(0x6636, 0x00);
	write_cmos_sensor(0x6637, 0x14);
	write_cmos_sensor(0x3066, 0x00);
	write_cmos_sensor(0x7B63, 0x00);
	write_cmos_sensor(0x4024, 0x0A);
	write_cmos_sensor(0x4025, 0xB8);
	write_cmos_sensor(0x56FB, 0x33);
	write_cmos_sensor(0x56FF, 0x33);
	write_cmos_sensor(0x6174, 0x29);
	write_cmos_sensor(0x6175, 0x29);
	write_cmos_sensor(0x910A, 0x00);
	write_cmos_sensor(0x9323, 0x15);
	write_cmos_sensor(0xBC60, 0x01);
	write_cmos_sensor(0x0202, 0x08);
	write_cmos_sensor(0x0203, 0x76);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0218, 0x01);
	write_cmos_sensor(0x0219, 0x00);

	pr_debug("L\n");

}				/*    preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	pr_debug("E! currefps:%d\n", currefps);

	write_cmos_sensor(0x0111, 0x02);
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0342, 0x18);
	write_cmos_sensor(0x0343, 0x50);
	write_cmos_sensor(0x0340, 0x10);
	write_cmos_sensor(0x0341, 0xB8);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x15);
	write_cmos_sensor(0x0349, 0x6F);
	write_cmos_sensor(0x034A, 0x10);
	write_cmos_sensor(0x034B, 0x0F);
	write_cmos_sensor(0x31A2, 0x00);
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0221, 0x11);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3010, 0x65);
	write_cmos_sensor(0x3011, 0x11);
	write_cmos_sensor(0x3194, 0x01);
	write_cmos_sensor(0x31A0, 0x00);
	write_cmos_sensor(0x31A1, 0x00);
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x15);
	write_cmos_sensor(0x040D, 0x70);
	write_cmos_sensor(0x040E, 0x10);
	write_cmos_sensor(0x040F, 0x10);
	write_cmos_sensor(0x034C, 0x15);
	write_cmos_sensor(0x034D, 0x70);
	write_cmos_sensor(0x034E, 0x10);
	write_cmos_sensor(0x034F, 0x10);
	write_cmos_sensor(0x0301, 0x05);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x01);
	write_cmos_sensor(0x0307, 0x4D);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x01);
	write_cmos_sensor(0x030F, 0x40);
	write_cmos_sensor(0x0820, 0x1E);
	write_cmos_sensor(0x0821, 0x00);
	write_cmos_sensor(0x0822, 0x00);
	write_cmos_sensor(0x0823, 0x00);
	write_cmos_sensor(0x3031, 0x00);
	write_cmos_sensor(0x3033, 0x00);
	write_cmos_sensor(0x3035, 0x00);
	write_cmos_sensor(0x3037, 0x00);
	write_cmos_sensor(0x3039, 0x00);
	write_cmos_sensor(0x303B, 0x00);
	write_cmos_sensor(0x306C, 0x00);
	write_cmos_sensor(0x306E, 0x0D);
	write_cmos_sensor(0x306F, 0x56);
	write_cmos_sensor(0x6636, 0x00);
	write_cmos_sensor(0x6637, 0x14);
	write_cmos_sensor(0x3066, 0x00);
	write_cmos_sensor(0x7B63, 0x00);
	write_cmos_sensor(0x0202, 0x10);
	write_cmos_sensor(0x0203, 0xA4);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0218, 0x01);
	write_cmos_sensor(0x0219, 0x00);
	write_cmos_sensor(0x56FA, 0x00);
	write_cmos_sensor(0x56FB, 0x50);
	write_cmos_sensor(0x56FE, 0x00);
	write_cmos_sensor(0x56FF, 0x50);
	write_cmos_sensor(0x9323, 0x10);

}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("E! currefps:%d\n", currefps);

	write_cmos_sensor(0x0111, 0x02);
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0342, 0x1C);
	write_cmos_sensor(0x0343, 0xB8);
	write_cmos_sensor(0x0340, 0x0C);
	write_cmos_sensor(0x0341, 0x40);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x02);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x15);
	write_cmos_sensor(0x0349, 0x6F);
	write_cmos_sensor(0x034A, 0x0E);
	write_cmos_sensor(0x034B, 0x0F);
	write_cmos_sensor(0x31A2, 0x00);
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0221, 0x11);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3010, 0x65);
	write_cmos_sensor(0x3011, 0x11);
	write_cmos_sensor(0x301C, 0x00);
	write_cmos_sensor(0x3045, 0x01);
	write_cmos_sensor(0x3194, 0x00);
	write_cmos_sensor(0x31A0, 0x00);
	write_cmos_sensor(0x31A1, 0x00);
	write_cmos_sensor(0xD5EC, 0x3A);
	write_cmos_sensor(0xD5ED, 0x00);
	write_cmos_sensor(0x0401, 0x02);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x16);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x68);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x3A);
	write_cmos_sensor(0x040C, 0x14);
	write_cmos_sensor(0x040D, 0xA0);
	write_cmos_sensor(0x040E, 0x0B);
	write_cmos_sensor(0x040F, 0x9A);
	write_cmos_sensor(0x034C, 0x0F);
	write_cmos_sensor(0x034D, 0x00);
	write_cmos_sensor(0x034E, 0x08);
	write_cmos_sensor(0x034F, 0x70);
	write_cmos_sensor(0x0301, 0x05);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x01);
	write_cmos_sensor(0x0307, 0x21);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x02);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x01);
	write_cmos_sensor(0x030F, 0x50);
	write_cmos_sensor(0x0820, 0x0F);
	write_cmos_sensor(0x0821, 0xC0);
	write_cmos_sensor(0x0822, 0x00);
	write_cmos_sensor(0x0823, 0x00);
	write_cmos_sensor(0x422f, 0x01);
	write_cmos_sensor(0x4230, 0x00);
	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3033, 0x00);
	write_cmos_sensor(0x3039, 0x00);
	write_cmos_sensor(0x303B, 0x00);
	write_cmos_sensor(0x306C, 0x00);
	write_cmos_sensor(0x306E, 0x0D);
	write_cmos_sensor(0x306F, 0x56);
	write_cmos_sensor(0x6636, 0x00);
	write_cmos_sensor(0x6637, 0x14);
	write_cmos_sensor(0xCA66, 0x39);
	write_cmos_sensor(0xCA67, 0x39);
	write_cmos_sensor(0xCA68, 0x39);
	write_cmos_sensor(0xCA69, 0x39);
	write_cmos_sensor(0xCA6A, 0x13);
	write_cmos_sensor(0xCA6B, 0x13);
	write_cmos_sensor(0xCA6C, 0x20);
	write_cmos_sensor(0xCA6D, 0x20);
	write_cmos_sensor(0xCA6E, 0x20);
	write_cmos_sensor(0xCA6F, 0x20);
	write_cmos_sensor(0xCA70, 0x10);
	write_cmos_sensor(0xCA71, 0x10);
	write_cmos_sensor(0x3066, 0x00);
	write_cmos_sensor(0x7B63, 0x00);
	write_cmos_sensor(0x4024, 0x0A);
	write_cmos_sensor(0x4025, 0xB8);
	write_cmos_sensor(0x56FB, 0x33);
	write_cmos_sensor(0x56FF, 0x33);
	write_cmos_sensor(0x6174, 0x29);
	write_cmos_sensor(0x6175, 0x02);
	write_cmos_sensor(0x910A, 0x00);
	write_cmos_sensor(0x9323, 0x15);
	write_cmos_sensor(0xBC60, 0x01);
	write_cmos_sensor(0x0202, 0x0C);
	write_cmos_sensor(0x0203, 0x36);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0218, 0x01);
	write_cmos_sensor(0x0219, 0x00);


}

#ifndef HIGH_SPEED_240FPS
static void hs_video_setting(void)
{
	pr_debug("E\n");


	write_cmos_sensor(0x0111, 0x02);
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);

	write_cmos_sensor(0x0342, 0x0E);
	write_cmos_sensor(0x0343, 0x60);

	write_cmos_sensor(0x0340, 0x06);
	write_cmos_sensor(0x0341, 0x38);

	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x02);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x15);
	write_cmos_sensor(0x0349, 0x6F);
	write_cmos_sensor(0x034A, 0x0E);
	write_cmos_sensor(0x034B, 0x0F);
	write_cmos_sensor(0x31A2, 0x00);

	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0221, 0x11);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x22);
	write_cmos_sensor(0x0902, 0x02);
	write_cmos_sensor(0x3010, 0x65);
	write_cmos_sensor(0x3011, 0x11);
	write_cmos_sensor(0x301C, 0x00);
	write_cmos_sensor(0x3045, 0x00);
	write_cmos_sensor(0x3194, 0x00);
	write_cmos_sensor(0x31A0, 0x00);
	write_cmos_sensor(0x31A1, 0x02);
	write_cmos_sensor(0xD5EC, 0x3A);
	write_cmos_sensor(0xD5ED, 0x00);

	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x0A);
	write_cmos_sensor(0x040D, 0xB8);
	write_cmos_sensor(0x040E, 0x06);
	write_cmos_sensor(0x040F, 0x08);

	write_cmos_sensor(0x034C, 0x0A);
	write_cmos_sensor(0x034D, 0xB8);
	write_cmos_sensor(0x034E, 0x06);
	write_cmos_sensor(0x034F, 0x08);

	write_cmos_sensor(0x0301, 0x05);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x01);
	write_cmos_sensor(0x0307, 0x25);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x00);
	write_cmos_sensor(0x030F, 0xFF);
	write_cmos_sensor(0x0820, 0x17);
	write_cmos_sensor(0x0821, 0xE8);
	write_cmos_sensor(0x0822, 0x00);
	write_cmos_sensor(0x0823, 0x00);
	write_cmos_sensor(0x422f, 0x01);
	write_cmos_sensor(0x4230, 0x00);

	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3033, 0x00);
	write_cmos_sensor(0x3039, 0x00);
	write_cmos_sensor(0x303B, 0x00);

	write_cmos_sensor(0x306C, 0x00);
	write_cmos_sensor(0x306E, 0x0D);
	write_cmos_sensor(0x306F, 0x56);
	write_cmos_sensor(0x6636, 0x00);
	write_cmos_sensor(0x6637, 0x14);
#if 0
	write_cmos_sensor(0xCA12, 0x2C);
	write_cmos_sensor(0xCA13, 0x2C);
	write_cmos_sensor(0xCA14, 0x1C);
	write_cmos_sensor(0xCA15, 0x1C);
	write_cmos_sensor(0xCA16, 0x06);
	write_cmos_sensor(0xCA17, 0x06);
	write_cmos_sensor(0xCA18, 0x20);
	write_cmos_sensor(0xCA19, 0x20);
	write_cmos_sensor(0xCA1A, 0x0C);
	write_cmos_sensor(0xCA1B, 0x0C);
	write_cmos_sensor(0xCA1C, 0x06);
	write_cmos_sensor(0xCA1D, 0x06);
	write_cmos_sensor(0xCA66, 0x39);
	write_cmos_sensor(0xCA67, 0x39);
	write_cmos_sensor(0xCA68, 0x39);
	write_cmos_sensor(0xCA69, 0x39);
	write_cmos_sensor(0xCA6A, 0x13);
	write_cmos_sensor(0xCA6B, 0x13);
	write_cmos_sensor(0xCA6C, 0x20);
	write_cmos_sensor(0xCA6D, 0x20);
	write_cmos_sensor(0xCA6E, 0x20);
	write_cmos_sensor(0xCA6F, 0x20);
	write_cmos_sensor(0xCA70, 0x10);
	write_cmos_sensor(0xCA71, 0x10);

	write_cmos_sensor(0x3900, 0x00);
	write_cmos_sensor(0x3901, 0x00);
	write_cmos_sensor(0x31C3, 0x01);
#endif
	write_cmos_sensor(0x3066, 0x00);

	write_cmos_sensor(0x7B63, 0x00);

	write_cmos_sensor(0x56FB, 0x33);
	write_cmos_sensor(0x56FF, 0x33);
	write_cmos_sensor(0x6174, 0x29);
	write_cmos_sensor(0x6175, 0x02);
	write_cmos_sensor(0x9323, 0x15);
	write_cmos_sensor(0xBC60, 0x01);

	write_cmos_sensor(0x30F1, 0x00);
	write_cmos_sensor(0x30F4, 0x00);
	write_cmos_sensor(0x30F5, 0xC8);
	write_cmos_sensor(0x30F6, 0x00);
	write_cmos_sensor(0x30F7, 0x14);
	write_cmos_sensor(0x30FC, 0x00);
	write_cmos_sensor(0x30FD, 0x00);
	write_cmos_sensor(0x714E, 0x00);
	write_cmos_sensor(0x714D, 0x08);
	write_cmos_sensor(0x7152, 0x0F);
	write_cmos_sensor(0x7156, 0x00);
	write_cmos_sensor(0x7155, 0x78);
	write_cmos_sensor(0x7159, 0x03);
	write_cmos_sensor(0x76A3, 0x08);
	write_cmos_sensor(0x76A0, 0x00);
	write_cmos_sensor(0x76A5, 0x0F);
	write_cmos_sensor(0x76A9, 0x78);
	write_cmos_sensor(0x76AF, 0x03);
	write_cmos_sensor(0x76AC, 0x00);
	write_cmos_sensor(0x9303, 0x32);
	write_cmos_sensor(0xBC62, 0x00);
	write_cmos_sensor(0xBC63, 0x38);
	write_cmos_sensor(0xD00C, 0x10);

	write_cmos_sensor(0x0202, 0x06);
	write_cmos_sensor(0x0203, 0x2E);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);

	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0218, 0x01);
	write_cmos_sensor(0x0219, 0x00);

}
#else

static void hs_video_setting(void)
{

	write_cmos_sensor(0x0111, 0x02);
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);

	write_cmos_sensor(0x0342, 0x0E);
	write_cmos_sensor(0x0343, 0x60);

	write_cmos_sensor(0x0340, 0x03);
	write_cmos_sensor(0x0341, 0x34);

	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x02);
	write_cmos_sensor(0x0347, 0x04);
	write_cmos_sensor(0x0348, 0x15);
	write_cmos_sensor(0x0349, 0x6F);
	write_cmos_sensor(0x034A, 0x0E);
	write_cmos_sensor(0x034B, 0x13);
	write_cmos_sensor(0x31A2, 0x00);

	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0221, 0x11);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x44);
	write_cmos_sensor(0x0902, 0x02);
	write_cmos_sensor(0x3010, 0x65);
	write_cmos_sensor(0x3011, 0x11);
	write_cmos_sensor(0x301C, 0x00);
	write_cmos_sensor(0x3045, 0x00);
	write_cmos_sensor(0x3194, 0x00);
	write_cmos_sensor(0x31A0, 0x00);
	write_cmos_sensor(0x31A1, 0x02);
	write_cmos_sensor(0xD5EC, 0x3A);
	write_cmos_sensor(0xD5ED, 0x00);

	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x05);
	write_cmos_sensor(0x040D, 0x5C);
	write_cmos_sensor(0x040E, 0x03);
	write_cmos_sensor(0x040F, 0x04);

	write_cmos_sensor(0x034C, 0x05);
	write_cmos_sensor(0x034D, 0x5C);
	write_cmos_sensor(0x034E, 0x03);
	write_cmos_sensor(0x034F, 0x04);

	write_cmos_sensor(0x0301, 0x05);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x01);
	write_cmos_sensor(0x0307, 0x2E);
	write_cmos_sensor(0x0309, 0x0A);
	write_cmos_sensor(0x030B, 0x02);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x01);
	write_cmos_sensor(0x030F, 0x28);
	write_cmos_sensor(0x0820, 0x0D);
	write_cmos_sensor(0x0821, 0xE0);
	write_cmos_sensor(0x0822, 0x00);
	write_cmos_sensor(0x0823, 0x00);
	write_cmos_sensor(0x422f, 0x01);
	write_cmos_sensor(0x4230, 0x00);

	write_cmos_sensor(0x3031, 0x01);
	write_cmos_sensor(0x3033, 0x00);
	write_cmos_sensor(0x3039, 0x00);
	write_cmos_sensor(0x303B, 0x00);

	write_cmos_sensor(0x306C, 0x00);
	write_cmos_sensor(0x306E, 0x0D);
	write_cmos_sensor(0x306F, 0x56);
	write_cmos_sensor(0x6636, 0x00);
	write_cmos_sensor(0x6637, 0x14);
#if 0
	write_cmos_sensor(0xCA12, 0x2C);
	write_cmos_sensor(0xCA13, 0x2C);
	write_cmos_sensor(0xCA14, 0x1C);
	write_cmos_sensor(0xCA15, 0x1C);
	write_cmos_sensor(0xCA16, 0x06);
	write_cmos_sensor(0xCA17, 0x06);
	write_cmos_sensor(0xCA18, 0x20);
	write_cmos_sensor(0xCA19, 0x20);
	write_cmos_sensor(0xCA1A, 0x0C);
	write_cmos_sensor(0xCA1B, 0x0C);
	write_cmos_sensor(0xCA1C, 0x06);
	write_cmos_sensor(0xCA1D, 0x06);
	write_cmos_sensor(0xCA66, 0x39);
	write_cmos_sensor(0xCA67, 0x39);
	write_cmos_sensor(0xCA68, 0x39);
	write_cmos_sensor(0xCA69, 0x39);
	write_cmos_sensor(0xCA6A, 0x13);
	write_cmos_sensor(0xCA6B, 0x13);
	write_cmos_sensor(0xCA6C, 0x20);
	write_cmos_sensor(0xCA6D, 0x20);
	write_cmos_sensor(0xCA6E, 0x20);
	write_cmos_sensor(0xCA6F, 0x20);
	write_cmos_sensor(0xCA70, 0x10);
	write_cmos_sensor(0xCA71, 0x10);

	write_cmos_sensor(0x3900, 0x00);
	write_cmos_sensor(0x3901, 0x00);
	write_cmos_sensor(0x31C3, 0x01);
#endif
	write_cmos_sensor(0x3066, 0x00);

	write_cmos_sensor(0x7B63, 0x00);

	write_cmos_sensor(0x56FB, 0x33);
	write_cmos_sensor(0x56FF, 0x33);
	write_cmos_sensor(0x6174, 0x29);
	write_cmos_sensor(0x6175, 0x02);
	write_cmos_sensor(0x9323, 0x15);
	write_cmos_sensor(0xBC60, 0x01);

	write_cmos_sensor(0x30F1, 0x00);
	write_cmos_sensor(0x30F4, 0x00);
	write_cmos_sensor(0x30F5, 0xC8);
	write_cmos_sensor(0x30F6, 0x00);
	write_cmos_sensor(0x30F7, 0x14);
	write_cmos_sensor(0x30FC, 0x00);
	write_cmos_sensor(0x30FD, 0x00);
	write_cmos_sensor(0x714E, 0x00);
	write_cmos_sensor(0x714D, 0x08);
	write_cmos_sensor(0x7152, 0x0F);
	write_cmos_sensor(0x7156, 0x00);
	write_cmos_sensor(0x7155, 0x78);
	write_cmos_sensor(0x7159, 0x03);
	write_cmos_sensor(0x76A3, 0x08);
	write_cmos_sensor(0x76A0, 0x00);
	write_cmos_sensor(0x76A5, 0x0F);
	write_cmos_sensor(0x76A9, 0x78);
	write_cmos_sensor(0x76AF, 0x03);
	write_cmos_sensor(0x76AC, 0x00);
	write_cmos_sensor(0x9303, 0x32);
	write_cmos_sensor(0xBC62, 0x00);
	write_cmos_sensor(0xBC63, 0x38);
	write_cmos_sensor(0xD00C, 0x10);

	write_cmos_sensor(0x0202, 0x03);
	write_cmos_sensor(0x0203, 0x2A);
	write_cmos_sensor(0x0225, 0xF4);

	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0210, 0x01);
	write_cmos_sensor(0x0211, 0x00);
	write_cmos_sensor(0x0212, 0x01);
	write_cmos_sensor(0x0213, 0x00);
	write_cmos_sensor(0x0214, 0x01);
	write_cmos_sensor(0x0215, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0218, 0x01);
	write_cmos_sensor(0x0219, 0x00);

}
#endif
static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor(0x0100, 0X01);
	else
		write_cmos_sensor(0x0100, 0x00);
	return ERROR_NONE;
}
static void slim_video_setting(void)
{
	pr_debug("E\n");
	/* @@video_720p_30fps_800Mbps */
	/*ToDo */
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
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			pr_debug(
			      "Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	imx318_apply_SPC();

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

	pr_debug("IMX318,MIPI 4LANE\n");

	pr_debug(
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
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

		pr_debug("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
	pr_debug("E\n");

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
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
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
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			pr_debug(
		"current_fps %d fps is not support, so use cap's setting: %d fps!\n",
		  imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);	/*Full mode */

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

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
	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

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
	pr_debug("E\n");

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
	pr_debug("E\n");
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
	pr_debug("scenario_id = %d\n", scenario_id);



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
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode */
	sensor_info->PDAF_Support = 0;

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
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{				/* This Function not used after ROME */
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
		/* set_dummy(); */
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
		/* set_dummy(); */
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
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

		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
			pr_debug(
	  "current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			     framerate, imgsensor_info.cap.max_framerate / 10);
		}

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
		/* set_dummy(); */
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
		/* set_dummy(); */
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
		/* set_dummy(); */
		break;

	default:		/* coding with  preview scenario by default */
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
		/* set_dummy(); */

		pr_debug("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_debug("scenario_id = %d\n", scenario_id);

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


static kal_uint32 imx318_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	UINT32 rgain_32, grgain_32, gbgain_32, bgain_32;

	pr_debug("%s\n", __func__);

	grgain_32 = (pSetSensorAWB->ABS_GAIN_GR << 8) >> 9;
	rgain_32 = (pSetSensorAWB->ABS_GAIN_R << 8) >> 9;
	bgain_32 = (pSetSensorAWB->ABS_GAIN_B << 8) >> 9;
	gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB << 8) >> 9;

	pr_debug("[%s] ABS_GAIN_GR:%d, grgain_32:%d\n", __func__,
		pSetSensorAWB->ABS_GAIN_GR, grgain_32);

	pr_debug("[%s] ABS_GAIN_R:%d, rgain_32:%d\n", __func__,
		pSetSensorAWB->ABS_GAIN_R, rgain_32);

	pr_debug("[%] ABS_GAIN_B:%d, bgain_32:%d\n", __func__,
		pSetSensorAWB->ABS_GAIN_B, bgain_32);

	pr_debug("[%] ABS_GAIN_GB:%d, gbgain_32:%d\n", __func__,
		pSetSensorAWB->ABS_GAIN_GB,	gbgain_32);

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

	if (temperature >= 0x0 && temperature <= 0x77)
		temperature_convert = temperature;
	else if (temperature >= 0x78 && temperature <= 0x7F)
		temperature_convert = 120;
	else if (temperature >= 0x80 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (INT8) temperature;

		pr_debug("temp_c(%d), read_reg(%d)\n",
			temperature_convert, temperature);

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
	/* unsigned long long *feature_return_data =
	 * (unsigned long long*)feature_para;
	 */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;

	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB =
		(struct SET_SENSOR_AWB_GAIN *) feature_para;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/* pr_debug("feature_id = %d\n", feature_id); */
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
		set_shutter((UINT16) *feature_data);
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
		write_cmos_sensor(
			sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;

	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;

	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
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
	      (enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;

	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		/* read_imx318_DCC((kal_uint16 )(*feature_data),
		 * (char*)(uintptr_t)(*(feature_data+1)),
		 * (kal_uint32)(*(feature_data+2)));
		 */
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
		pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

		ihdr_write_shutter_gain(*feature_data,
				*(feature_data + 1), *(feature_data + 2));
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
		imx318_awb_gain(pSetSensorAWB);
		break;
		/*END OF HDR CMD */
		/*PDAF CMD */
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n",
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
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
		/*End of PDAF */
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		if (imgsensor_info.temperature_support != 0) {
			*feature_return_para_i32 = get_sensor_temperature();
			*feature_para_len = 4;
		}
		break;

	case SENSOR_FEATURE_SET_PDFOCUS_AREA:
		pr_debug(
	    "SENSOR_FEATURE_SET_IMX318_PDFOCUS_AREA Start Pos=%d, Size=%d\n",
			(UINT32) *feature_data, (UINT32) *(feature_data + 1));

		imx318_set_pd_focus_area(*feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;

	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);

		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
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

UINT32 IMX318_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    IMX230_MIPI_RAW_SensorInit    */
