// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX486mipiraw_Sensor.c
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

/********************Modify Following Strings for Debug***********************/
#define PFX "IMX486_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__
/****************************   Modify end    *******************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "imx486mipiraw_Sensor.h"
#define LOG_INF(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)

#define MULTI_WRITE 1
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {

	/* IMX486MIPI_SENSOR_ID, sensor_id = 0x2680*/
	.sensor_id = IMX486_SENSOR_ID,

	.checksum_value = 0x38ebe79e, /* checksum value for Camera Auto Test */

	.pre = {
		.pclk = 168000000,	/* record different mode's pclk */
		.linelength = 2096,	/* record different mode's linelength */
		.framelength = 2670,  /* record different mode's framelength */
		.startx = 0, /* record different mode's startx of grabwindow */
		.starty = 0, /* record different mode's starty of grabwindow */

		/* record different mode's width of grabwindow */
		.grabwindow_width = 2016,

		/* record different mode's height of grabwindow */
		.grabwindow_height = 1508,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 14,	/* unit , ns */
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
	},
	.cap = {		/*normal capture */
		.pclk = 386000000,
		.linelength = 4192,
		.framelength = 3068,	/* 1332, */
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4032,
		.grabwindow_height = 3016,
		.mipi_data_lp2hs_settle_dc = 14,	/* unit , ns */
		.max_framerate = 300,	/* 300, */
	},
	.cap1 = {		/*PIP capture */
		 .pclk = 194000000,
		 .linelength = 4192,
		 .framelength = 3068,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 4032,
		 .grabwindow_height = 3016,
		 .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		 .max_framerate = 150,
	},
	.normal_video = {
			 .pclk = 386000000,
			 .linelength = 4192,
			 .framelength = 3068,
			 .startx = 0,
			 .starty = 0,
			 .grabwindow_width = 4032,
			 .grabwindow_height = 3016,
			 .mipi_data_lp2hs_settle_dc = 14,	/* unit , ns */
			 .max_framerate = 300,	/* modify */
	},
	.hs_video = {		/*slow motion */
		     .pclk = 300000000,	/* 518400000, */
		     .linelength = 2096,
		     .framelength = 1100,	/* 806, */
		     .startx = 0,
		     .starty = 0,
		     .grabwindow_width = 1280,	/* 1400, */
		     .grabwindow_height = 720,	/* 752, */
		     .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		     .max_framerate = 1280,	/* modify */

	},
	.slim_video = {		/*VT Call */
		       .pclk = 168000000,	/* 158400000, */
		       .linelength = 2096,
		       .framelength = 2670,	/* 986,      //1236 */
		       .startx = 0,
		       .starty = 0,
		       .grabwindow_width = 2016,	/* 1400, */
		       .grabwindow_height = 1508,	/* 752, */
		       .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		       .max_framerate = 300,
	},
	.custom1 = {		/*custom1 */
		       .pclk = 168000000,
		       .linelength = 2096,
		       .framelength = 2670,
		       .startx = 0,
		       .starty = 0,
		       .grabwindow_width = 2016,
		       .grabwindow_height = 1508,
		       .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		       .max_framerate = 300,
	},
	.custom2 = {		/* custom2 */
		       .pclk = 386000000,
		       .linelength = 4192,
		       .framelength = 3068,
		       .startx = 0,
		       .starty = 0,
		       .grabwindow_width = 4032,
		       .grabwindow_height = 3016,
		       .mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		       .max_framerate = 300,
	},
	.margin = 10,/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* 1,          //min shutter */

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0xff00,
	.ae_shut_delay_frame = 0,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_ispGain_delay_frame = 2, /* isp gain delay frame for AE cycle */

	/* The delay frame of setting frame length	*/
	.frame_time_delay_frame = 2,

	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 7,	/* support sensor mode num */

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2, /* enter high speed video  delay frame num */
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	/* sensor output first pixel color */
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */

/* record sensor support all write id addr, only supprt 4must end with 0xff */
	.i2c_addr_table = {0x20, 0x34, 0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x14d,	/* current shutter */
	.gain = 0x0000,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */

	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	.hdr_mode = 0,	/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34,	/* record current sensor's i2c write id */
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{4032, 3016, 0, 0, 4032, 3016, 2016, 1508,
	 0000, 0000, 2016, 1508, 0, 0, 2016, 1508},	/* Preview */
	{4032, 3016, 0, 0, 4032, 3016, 4032, 3016,
	 0000, 0000, 4032, 3016, 0, 0, 4032, 3016},	/*capture */
	{4032, 3016, 0, 0, 4032, 3016, 4032, 3016,
	 0000, 0000, 4032, 3016, 0, 0, 4032, 3016},	/*video */
	{4032, 3016, 0, 0, 4032, 3016, 1280, 720,
	 0000, 0000, 1280, 720, 0, 0, 1280, 720},	/*hight speed video */
	{4032, 3016, 0, 0, 4032, 3016, 2016, 1508,
	 0000, 0000, 2016, 1508, 0, 0, 2016, 1508},	/*slim video */
	{4032, 3016, 0, 0, 4032, 3016, 2016, 1508,
	 0000, 0000, 2016, 1508, 0, 0, 2016, 1508}, /* Custom1 */
	{4032, 3016, 0, 0, 4032, 3016, 4032, 3016,
	 0000, 0000, 4032, 3016, 0, 0, 4032, 3016}, /* Custom2 */
};

/* #define RAW_TYPE_OVERRIDE     //it's PDO function define */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 64,
	.i4OffsetY = 16,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 4,
	.i4SubBlkW = 16,
	.i4SubBlkH = 16,
	.i4BlockNumX = 122,
	.i4BlockNumY = 93,
	.i4PosL = {{67, 21}, {83, 21}, {75, 41}, {91, 41} },
	.i4PosR = {{67, 25}, {83, 25}, {75, 37}, {91, 37} },
	/* 0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR */
	.iMirrorFlip = 0,
};

#ifdef MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3
#endif

static kal_uint16 imx486_table_write_cmos_sensor(
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

#define IMX486MIPI_MaxGainIndex (193)
kal_uint16 IMX486MIPI_sensorGainMapping[IMX486MIPI_MaxGainIndex][2] = {
	{64, 0},
	{66, 16},
	{68, 30},
	{70, 44},
	{72, 57},
	{74, 69},
	{76, 81},
	{78, 92},
	{80, 102},
	{82, 112},
	{84, 122},
	{86, 131},
	{88, 140},
	{90, 148},
	{92, 156},
	{94, 163},
	{96, 171},
	{98, 178},
	{100, 184},
	{102, 191},
	{104, 197},
	{106, 203},
	{108, 209},
	{110, 214},
	{112, 219},
	{114, 225},
	{116, 230},
	{118, 234},
	{120, 239},
	{122, 243},
	{124, 248},
	{126, 252},
	{128, 256},
	{130, 260},
	{132, 264},
	{134, 267},
	{136, 271},
	{138, 275},
	{140, 278},
	{142, 281},
	{144, 284},
	{146, 288},
	{148, 291},
	{150, 294},
	{152, 296},
	{154, 299},
	{156, 302},
	{158, 305},
	{160, 307},
	{162, 310},
	{164, 312},
	{166, 315},
	{168, 317},
	{170, 319},
	{172, 321},
	{174, 324},
	{176, 326},
	{178, 328},
	{180, 330},
	{182, 332},
	{184, 334},
	{186, 336},
	{188, 338},
	{191, 340},
	{192, 341},
	{194, 343},
	{196, 345},
	{199, 347},
	{200, 348},
	{202, 350},
	{204, 351},
	{206, 353},
	{207, 354},
	{210, 356},
	{211, 357},
	{214, 359},
	{216, 360},
	{218, 362},
	{220, 363},
	{221, 364},
	{224, 366},
	{226, 367},
	{228, 368},
	{231, 370},
	{232, 371},
	{234, 372},
	{236, 373},
	{237, 374},
	{239, 375},
	{243, 377},
	{245, 378},
	{246, 379},
	{248, 380},
	{250, 381},
	{252, 382},
	{254, 383},
	{256, 384},
	{258, 385},
	{260, 386},
	{262, 387},
	{264, 388},
	{266, 389},
	{269, 390},
	{271, 391},
	{273, 392},
	{275, 393},
	{278, 394},
	{280, 395},
	{283, 396},
	{285, 397},
	{287, 398},
	{290, 399},
	{293, 400},
	{295, 401},
	{298, 402},
	{301, 403},
	{303, 404},
	{306, 405},
	{309, 406},
	{312, 407},
	{315, 408},
	{318, 409},
	{321, 410},
	{324, 411},
	{328, 412},
	{331, 413},
	{334, 414},
	{338, 415},
	{341, 416},
	{345, 417},
	{349, 418},
	{352, 419},
	{356, 420},
	{360, 421},
	{364, 422},
	{368, 423},
	{372, 424},
	{377, 425},
	{381, 426},
	{386, 427},
	{390, 428},
	{395, 429},
	{400, 430},
	{405, 431},
	{410, 432},
	{415, 433},
	{420, 434},
	{426, 435},
	{431, 436},
	{437, 437},
	{443, 438},
	{449, 439},
	{455, 440},
	{462, 441},
	{468, 442},
	{475, 443},
	{482, 444},
	{489, 445},
	{497, 446},
	{504, 447},
	{512, 448},
	{520, 449},
	{529, 450},
	{537, 451},
	{546, 452},
	{555, 453},
	{565, 454},
	{575, 455},
	{585, 456},
	{596, 457},
	{607, 458},
	{618, 459},
	{630, 460},
	{643, 461},
	{655, 462},
	{669, 463},
	{683, 464},
	{697, 465},
	{712, 466},
	{728, 467},
	{745, 468},
	{762, 469},
	{780, 470},
	{799, 471},
	{819, 472},
	{840, 473},
	{862, 474},
	{886, 475},
	{910, 476},
	{936, 477},
	{964, 478},
	{993, 479},
	{1024, 480}
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
	int ret = 0;
	char pu_send_cmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	ret = iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);

	return ret;
}

static void set_dummy(void)
{
	/*
	 * LOG_INF("dummyline = %d, dummypixels = %d\n",
	 * imgsensor.dummy_line, imgsensor.dummy_pixel);
	 */

	write_cmos_sensor(0x0104, 0x01);

	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	//write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	//write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);

	write_cmos_sensor(0x0104, 0x00);
}

static kal_uint32 return_sensor_id(void)
{
/* kal_uint32 tmp = 0; */
	int retry = 10;

	if (write_cmos_sensor(0x0A02, 0x0b) == 0) {
		write_cmos_sensor(0x0A00, 0x01);
		while (retry--) {
			if (read_cmos_sensor(0x0A01) == 0x01)
				return (kal_uint16)
					((read_cmos_sensor(0x0A29) << 4) |
					 (read_cmos_sensor(0x0A2A) >> 4));
		}
	}

	return 0x00;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
	    (frame_length > imgsensor.min_frame_length)
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
}				/*      set_max_framerate  */


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
	write_cmos_sensor(0x0350, 0x01);	/* Enable auto extend */
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, shutter & 0xFF);
	write_cmos_sensor(0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d, auto_extend=%d\n",
		shutter, imgsensor.frame_length, read_cmos_sensor(0x0350));
}				/*    set_shutter */


static void set_shutter_frame_length(
				kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	/* LOG_INF("shutter =%d, frame_time =%d\n", shutter, frame_time); */

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
	/*Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	/*  */
	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

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
	write_cmos_sensor(0x0350, 0x00);/* Disable auto extend */
	write_cmos_sensor(0x0202,
		(shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, shutter & 0xFF);
	write_cmos_sensor(0x0104, 0x00);

	LOG_INF("shutter =%d,framelen =%d/%d,dummy_line=%d,autoextend=%d\n",
		shutter, imgsensor.frame_length, frame_length,
		dummy_line, read_cmos_sensor(0x0350));
}				/* set_shutter_frame_length */


static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 i;
#if 0
	int reg_gain = 0;

	reg_gain = 512-(100*512*64/gain)/100;
	if (reg_gain < 0)
		reg_gain = 0;
	if (reg_gain > 480)
		reg_gain = 480;
	return reg_gain;
#else
	for (i = 0; i < IMX486MIPI_MaxGainIndex; i++) {
		if (gain <= IMX486MIPI_sensorGainMapping[i][0])
			break;
	}
	if (gain != IMX486MIPI_sensorGainMapping[i][0])
		LOG_INF("Gain mapping don't correctly:%d %d\n", gain,
			IMX486MIPI_sensorGainMapping[i][0]);
	return IMX486MIPI_sensorGainMapping[i][1];
#endif
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
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X        */
	/* [4:9] = M meams M X             */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0104, 0x01);
	write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
	write_cmos_sensor(0x0205, reg_gain & 0xFF);
	write_cmos_sensor(0x0104, 0x00);

	return gain;
}				/*      set_gain  */

static void ihdr_write_shutter_gain(
				kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{

	kal_uint16 realtime_fps = 0;
	kal_uint16 reg_gain;

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
	write_cmos_sensor(0x0350, 0x01);	/* Enable auto extend */
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
	/* write_cmos_sensor(0x0216, (reg_gain>>8)& 0xFF); */
	/* write_cmos_sensor(0x0217, reg_gain & 0xFF); */
	write_cmos_sensor(0x0104, 0x00);
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x, auto_extend=%d\n", le, se, gain,
		read_cmos_sensor(0x0350));
}
#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 iTemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	iTemp = read_cmos_sensor(0x0101);
	iTemp &= ~0x03;		/* Clear the mirror and flip bits. */

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0101, iTemp | 0x00);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0101, iTemp | 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0101, iTemp | 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0101, iTemp | 0x03);
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
}				/*      night_mode      */
#if 0
	/*      preview_setting  */
static void imx486_ImageQuality_Setting(void)
{

}
#endif

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_imx486[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x4979, 0x0d,
	0x3040, 0x03,
	0x3041, 0x04,
	0x4340, 0x01,
	0x4341, 0x02,
	0x4342, 0x04,
	0x4344, 0x00,
	0x4345, 0x2C,
	0x4346, 0x00,
	0x4347, 0x26,
	0x434C, 0x00,
	0x434D, 0x31,
	0x434E, 0x00,
	0x434F, 0x2C,
	0x4350, 0x00,
	0x4351, 0x0A,
	0x4354, 0x00,
	0x4355, 0x2A,
	0x4356, 0x00,
	0x4357, 0x1F,
	0x435C, 0x00,
	0x435D, 0x2F,
	0x435E, 0x00,
	0x435F, 0x2A,
	0x4360, 0x00,
	0x4361, 0x0A,
	0x4600, 0x2A,
	0x4601, 0x30,
	0x4920, 0x0A,
	0x4926, 0x01,
	0x4929, 0x01,
	0x4938, 0x01,
	0x4959, 0x15,
	0x4A38, 0x0A,
	0x4A39, 0x05,
	0x4A3C, 0x0F,
	0x4A3D, 0x05,
	0x4A3E, 0x05,
	0x4A40, 0x0F,
	0x4A4A, 0x01,
	0x4A4B, 0x01,
	0x4A4C, 0x01,
	0x4A4D, 0x01,
	0x4A4E, 0x01,
	0x4A4F, 0x01,
	0x4A50, 0x01,
	0x4A51, 0x01,
	0x4A76, 0x16,
	0x4A77, 0x16,
	0x4A78, 0x16,
	0x4A79, 0x16,
	0x4A7A, 0x16,
	0x4A7B, 0x16,
	0x4A7C, 0x16,
	0x4A7D, 0x16,
	0x555E, 0x01,
	0x5563, 0x01,
	0x70DA, 0x02
};
#endif

static void sensor_init(void)
{
//From reg tool at 2018-1-6 16:54
#if MULTI_WRITE
	LOG_INF("%s MULTI_WRITE\n", __func__);
	imx486_table_write_cmos_sensor(
		addr_data_pair_init_imx486,
		sizeof(addr_data_pair_init_imx486) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0136, 0x18);
	write_cmos_sensor(0x0137, 0x00);
	write_cmos_sensor(0x4979, 0x0d);
	write_cmos_sensor(0x3040, 0x03);
	write_cmos_sensor(0x3041, 0x04);
	write_cmos_sensor(0x4340, 0x01);
	write_cmos_sensor(0x4341, 0x02);
	write_cmos_sensor(0x4342, 0x04);
	write_cmos_sensor(0x4344, 0x00);
	write_cmos_sensor(0x4345, 0x2C);
	write_cmos_sensor(0x4346, 0x00);
	write_cmos_sensor(0x4347, 0x26);
	write_cmos_sensor(0x434C, 0x00);
	write_cmos_sensor(0x434D, 0x31);
	write_cmos_sensor(0x434E, 0x00);
	write_cmos_sensor(0x434F, 0x2C);
	write_cmos_sensor(0x4350, 0x00);
	write_cmos_sensor(0x4351, 0x0A);
	write_cmos_sensor(0x4354, 0x00);
	write_cmos_sensor(0x4355, 0x2A);
	write_cmos_sensor(0x4356, 0x00);
	write_cmos_sensor(0x4357, 0x1F);
	write_cmos_sensor(0x435C, 0x00);
	write_cmos_sensor(0x435D, 0x2F);
	write_cmos_sensor(0x435E, 0x00);
	write_cmos_sensor(0x435F, 0x2A);
	write_cmos_sensor(0x4360, 0x00);
	write_cmos_sensor(0x4361, 0x0A);
	write_cmos_sensor(0x4600, 0x2A);
	write_cmos_sensor(0x4601, 0x30);
	write_cmos_sensor(0x4920, 0x0A);
	write_cmos_sensor(0x4926, 0x01);
	write_cmos_sensor(0x4929, 0x01);
	write_cmos_sensor(0x4938, 0x01);
	write_cmos_sensor(0x4959, 0x15);
	write_cmos_sensor(0x4A38, 0x0A);
	write_cmos_sensor(0x4A39, 0x05);
	write_cmos_sensor(0x4A3C, 0x0F);
	write_cmos_sensor(0x4A3D, 0x05);
	write_cmos_sensor(0x4A3E, 0x05);
	write_cmos_sensor(0x4A40, 0x0F);
	write_cmos_sensor(0x4A4A, 0x01);
	write_cmos_sensor(0x4A4B, 0x01);
	write_cmos_sensor(0x4A4C, 0x01);
	write_cmos_sensor(0x4A4D, 0x01);
	write_cmos_sensor(0x4A4E, 0x01);
	write_cmos_sensor(0x4A4F, 0x01);
	write_cmos_sensor(0x4A50, 0x01);
	write_cmos_sensor(0x4A51, 0x01);
	write_cmos_sensor(0x4A76, 0x16);
	write_cmos_sensor(0x4A77, 0x16);
	write_cmos_sensor(0x4A78, 0x16);
	write_cmos_sensor(0x4A79, 0x16);
	write_cmos_sensor(0x4A7A, 0x16);
	write_cmos_sensor(0x4A7B, 0x16);
	write_cmos_sensor(0x4A7C, 0x16);
	write_cmos_sensor(0x4A7D, 0x16);
	write_cmos_sensor(0x555E, 0x01);
	write_cmos_sensor(0x5563, 0x01);
	write_cmos_sensor(0x70DA, 0x02);
#endif
}				/*      sensor_init  */

#if MULTI_WRITE
kal_uint16 addr_data_pair_preview_imx486[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x08,
	0x0343, 0x30,
	0x0340, 0x0A,
	0x0341, 0x6E,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x0F,
	0x0349, 0xBF,
	0x034A, 0x0B,
	0x034B, 0xC7,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x3130, 0x01,
	0x034C, 0x07,
	0x034D, 0xE0,
	0x034E, 0x05,
	0x034F, 0xE4,
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0x54,
	0x030B, 0x01,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0x2A,
	0x0310, 0x01,
	0x0700, 0x01,
	0x0701, 0x50,
	0x0820, 0x07,
	0x0821, 0xE0,
	0x3100, 0x06,
	0x7036, 0x02,
	0x704E, 0x02,
	0x0202, 0x0A,
	0x0203, 0x64,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0226, 0x01,
	0x0227, 0x00
};
#endif

static void preview_setting(void)
{
#if MULTI_WRITE
	imx486_table_write_cmos_sensor(
		addr_data_pair_preview_imx486,
		sizeof(addr_data_pair_preview_imx486) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0342, 0x08);
	write_cmos_sensor(0x0343, 0x30);
	write_cmos_sensor(0x0340, 0x0A);
	write_cmos_sensor(0x0341, 0x6E);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0xBF);
	write_cmos_sensor(0x034A, 0x0B);
	write_cmos_sensor(0x034B, 0xC7);
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x22);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3130, 0x01);
	write_cmos_sensor(0x034C, 0x07);
	write_cmos_sensor(0x034D, 0xE0);
	write_cmos_sensor(0x034E, 0x05);
	write_cmos_sensor(0x034F, 0xE4);
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0x54);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x02);
	write_cmos_sensor(0x030E, 0x00);
	write_cmos_sensor(0x030F, 0x2A);
	write_cmos_sensor(0x0310, 0x01);
	write_cmos_sensor(0x0700, 0x01);
	write_cmos_sensor(0x0701, 0x50);
	write_cmos_sensor(0x0820, 0x07);
	write_cmos_sensor(0x0821, 0xE0);
	write_cmos_sensor(0x3100, 0x06);
	write_cmos_sensor(0x7036, 0x02);
	write_cmos_sensor(0x704E, 0x02);
	write_cmos_sensor(0x0202, 0x0A);
	write_cmos_sensor(0x0203, 0x64);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0226, 0x01);
	write_cmos_sensor(0x0227, 0x00);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_15fps_imx486[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x10,
	0x0343, 0x60,
	0x0340, 0x0B,
	0x0341, 0xFC,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x0F,
	0x0349, 0xBF,
	0x034A, 0x0B,
	0x034B, 0xC7,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x3130, 0x01,
	0x034C, 0x0F,
	0x034D, 0xC0,
	0x034E, 0x0B,
	0x034F, 0xC8,
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0x61,
	0x030B, 0x01,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0x2A,
	0x0310, 0x01,
	0x0700, 0x00,
	0x0701, 0x98,
	0x0820, 0x07,
	0x0821, 0xE0,
	0x3100, 0x06,
	0x7036, 0x02,
	0x704E, 0x02,
	0x0202, 0x0B,
	0x0203, 0xF2,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0226, 0x01,
	0x0227, 0x00
};

kal_uint16 addr_data_pair_capture_24fps_imx486[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x10,
	0x0343, 0x60,
	0x0340, 0x0B,
	0x0341, 0xFC,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x0F,
	0x0349, 0xBF,
	0x034A, 0x0B,
	0x034B, 0xC7,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x3130, 0x01,
	0x034C, 0x0F,
	0x034D, 0xC0,
	0x034E, 0x0B,
	0x034F, 0xC8,
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0x9C,
	0x030B, 0x01,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0x42,
	0x0310, 0x01,
	0x0700, 0x00,
	0x0701, 0x40,
	0x0820, 0x0C,
	0x0821, 0x60,
	0x3100, 0x06,
	0x7036, 0x00,
	0x704E, 0x02,
	0x0202, 0x0B,
	0x0203, 0xF2,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0226, 0x01,
	0x0227, 0x00
};

kal_uint16 addr_data_pair_capture_30fps_imx486[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x10,
	0x0343, 0x60,
	0x0340, 0x0B,
	0x0341, 0xFC,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x0F,
	0x0349, 0xBF,
	0x034A, 0x0B,
	0x034B, 0xC7,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x3130, 0x01,
	0x034C, 0x0F,
	0x034D, 0xC0,
	0x034E, 0x0B,
	0x034F, 0xC8,
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC1,
	0x030B, 0x01,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0x52,
	0x0310, 0x01,
	0x0700, 0x00,
	0x0701, 0x50,
	0x0820, 0x0F,
	0x0821, 0x60,
	0x3100, 0x06,
	0x7036, 0x00,
	0x704E, 0x00,
	0x0202, 0x0B,
	0x0203, 0xF2,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0226, 0x01,
	0x0227, 0x00
};
#endif


static void capture_setting(kal_uint16 curretfps, kal_uint8 pdaf_mode)
{
	LOG_INF("capture currefps:%d\n", curretfps);
#if MULTI_WRITE
	if (curretfps == 150) {
		imx486_table_write_cmos_sensor(
			addr_data_pair_capture_15fps_imx486,
			sizeof(addr_data_pair_capture_15fps_imx486) /
			sizeof(kal_uint16));
	} else if (curretfps == 240) {
		imx486_table_write_cmos_sensor(
			addr_data_pair_capture_24fps_imx486,
			sizeof(addr_data_pair_capture_24fps_imx486) /
			sizeof(kal_uint16));
	} else {
		imx486_table_write_cmos_sensor(
			addr_data_pair_capture_30fps_imx486,
			sizeof(addr_data_pair_capture_30fps_imx486) /
			sizeof(kal_uint16));
	}
#else
	if (curretfps == 150) {
		write_cmos_sensor(0x0112, 0x0A);
		write_cmos_sensor(0x0113, 0x0A);
		write_cmos_sensor(0x0114, 0x03);
		write_cmos_sensor(0x0342, 0x10);
		write_cmos_sensor(0x0343, 0x60);
		write_cmos_sensor(0x0340, 0x0B);
		write_cmos_sensor(0x0341, 0xFC);
		write_cmos_sensor(0x0344, 0x00);
		write_cmos_sensor(0x0345, 0x00);
		write_cmos_sensor(0x0346, 0x00);
		write_cmos_sensor(0x0347, 0x00);
		write_cmos_sensor(0x0348, 0x0F);
		write_cmos_sensor(0x0349, 0xBF);
		write_cmos_sensor(0x034A, 0x0B);
		write_cmos_sensor(0x034B, 0xC7);
		write_cmos_sensor(0x0220, 0x00);
		write_cmos_sensor(0x0222, 0x01);
		write_cmos_sensor(0x0900, 0x00);
		write_cmos_sensor(0x0901, 0x11);
		write_cmos_sensor(0x0902, 0x00);
		write_cmos_sensor(0x3130, 0x01);
		write_cmos_sensor(0x034C, 0x0F);
		write_cmos_sensor(0x034D, 0xC0);
		write_cmos_sensor(0x034E, 0x0B);
		write_cmos_sensor(0x034F, 0xC8);
		write_cmos_sensor(0x0301, 0x06);
		write_cmos_sensor(0x0303, 0x02);
		write_cmos_sensor(0x0305, 0x04);
		write_cmos_sensor(0x0306, 0x00);
		write_cmos_sensor(0x0307, 0x61);
		write_cmos_sensor(0x030B, 0x01);
		write_cmos_sensor(0x030D, 0x02);
		write_cmos_sensor(0x030E, 0x00);
		write_cmos_sensor(0x030F, 0x2A);
		write_cmos_sensor(0x0310, 0x01);
		write_cmos_sensor(0x0700, 0x00);
		write_cmos_sensor(0x0701, 0x98);
		write_cmos_sensor(0x0820, 0x07);
		write_cmos_sensor(0x0821, 0xE0);
		write_cmos_sensor(0x3100, 0x06);
		write_cmos_sensor(0x7036, 0x02);
		write_cmos_sensor(0x704E, 0x02);
		write_cmos_sensor(0x0202, 0x0B);
		write_cmos_sensor(0x0203, 0xF2);
		write_cmos_sensor(0x0224, 0x01);
		write_cmos_sensor(0x0225, 0xF4);
		write_cmos_sensor(0x0204, 0x00);
		write_cmos_sensor(0x0205, 0x00);
		write_cmos_sensor(0x020F, 0x00);
		write_cmos_sensor(0x0216, 0x00);
		write_cmos_sensor(0x0217, 0x00);
		write_cmos_sensor(0x0226, 0x01);
		write_cmos_sensor(0x0227, 0x00);
	} else if (curretfps == 240) {
		write_cmos_sensor(0x0112, 0x0A);
		write_cmos_sensor(0x0113, 0x0A);
		write_cmos_sensor(0x0114, 0x03);
		write_cmos_sensor(0x0342, 0x10);
		write_cmos_sensor(0x0343, 0x60);
		write_cmos_sensor(0x0340, 0x0B);
		write_cmos_sensor(0x0341, 0xFC);
		write_cmos_sensor(0x0344, 0x00);
		write_cmos_sensor(0x0345, 0x00);
		write_cmos_sensor(0x0346, 0x00);
		write_cmos_sensor(0x0347, 0x00);
		write_cmos_sensor(0x0348, 0x0F);
		write_cmos_sensor(0x0349, 0xBF);
		write_cmos_sensor(0x034A, 0x0B);
		write_cmos_sensor(0x034B, 0xC7);
		write_cmos_sensor(0x0220, 0x00);
		write_cmos_sensor(0x0222, 0x01);
		write_cmos_sensor(0x0900, 0x00);
		write_cmos_sensor(0x0901, 0x11);
		write_cmos_sensor(0x0902, 0x00);
		write_cmos_sensor(0x3130, 0x01);
		write_cmos_sensor(0x034C, 0x0F);
		write_cmos_sensor(0x034D, 0xC0);
		write_cmos_sensor(0x034E, 0x0B);
		write_cmos_sensor(0x034F, 0xC8);
		write_cmos_sensor(0x0301, 0x06);
		write_cmos_sensor(0x0303, 0x02);
		write_cmos_sensor(0x0305, 0x04);
		write_cmos_sensor(0x0306, 0x00);
		write_cmos_sensor(0x0307, 0x9C);
		write_cmos_sensor(0x030B, 0x01);
		write_cmos_sensor(0x030D, 0x02);
		write_cmos_sensor(0x030E, 0x00);
		write_cmos_sensor(0x030F, 0x42);
		write_cmos_sensor(0x0310, 0x01);
		write_cmos_sensor(0x0700, 0x00);
		write_cmos_sensor(0x0701, 0x40);
		write_cmos_sensor(0x0820, 0x0C);
		write_cmos_sensor(0x0821, 0x60);
		write_cmos_sensor(0x3100, 0x06);
		write_cmos_sensor(0x7036, 0x00);
		write_cmos_sensor(0x704E, 0x02);
		write_cmos_sensor(0x0202, 0x0B);
		write_cmos_sensor(0x0203, 0xF2);
		write_cmos_sensor(0x0224, 0x01);
		write_cmos_sensor(0x0225, 0xF4);
		write_cmos_sensor(0x0204, 0x00);
		write_cmos_sensor(0x0205, 0x00);
		write_cmos_sensor(0x020F, 0x00);
		write_cmos_sensor(0x0216, 0x00);
		write_cmos_sensor(0x0217, 0x00);
		write_cmos_sensor(0x0226, 0x01);
		write_cmos_sensor(0x0227, 0x00);
	} else {
		write_cmos_sensor(0x0112, 0x0A);
		write_cmos_sensor(0x0113, 0x0A);
		write_cmos_sensor(0x0114, 0x03);
		write_cmos_sensor(0x0342, 0x10);
		write_cmos_sensor(0x0343, 0x60);
		write_cmos_sensor(0x0340, 0x0B);
		write_cmos_sensor(0x0341, 0xFC);
		write_cmos_sensor(0x0344, 0x00);
		write_cmos_sensor(0x0345, 0x00);
		write_cmos_sensor(0x0346, 0x00);
		write_cmos_sensor(0x0347, 0x00);
		write_cmos_sensor(0x0348, 0x0F);
		write_cmos_sensor(0x0349, 0xBF);
		write_cmos_sensor(0x034A, 0x0B);
		write_cmos_sensor(0x034B, 0xC7);
		write_cmos_sensor(0x0220, 0x00);
		write_cmos_sensor(0x0222, 0x01);
		write_cmos_sensor(0x0900, 0x00);
		write_cmos_sensor(0x0901, 0x11);
		write_cmos_sensor(0x0902, 0x00);
		write_cmos_sensor(0x3130, 0x01);
		write_cmos_sensor(0x034C, 0x0F);
		write_cmos_sensor(0x034D, 0xC0);
		write_cmos_sensor(0x034E, 0x0B);
		write_cmos_sensor(0x034F, 0xC8);
		write_cmos_sensor(0x0301, 0x06);
		write_cmos_sensor(0x0303, 0x02);
		write_cmos_sensor(0x0305, 0x04);
		write_cmos_sensor(0x0306, 0x00);
		write_cmos_sensor(0x0307, 0xC1);
		write_cmos_sensor(0x030B, 0x01);
		write_cmos_sensor(0x030D, 0x02);
		write_cmos_sensor(0x030E, 0x00);
		write_cmos_sensor(0x030F, 0x52);
		write_cmos_sensor(0x0310, 0x01);
		write_cmos_sensor(0x0700, 0x00);
		write_cmos_sensor(0x0701, 0x50);
		write_cmos_sensor(0x0820, 0x0F);
		write_cmos_sensor(0x0821, 0x60);
		write_cmos_sensor(0x3100, 0x06);
		write_cmos_sensor(0x7036, 0x00);
		write_cmos_sensor(0x704E, 0x00);
		write_cmos_sensor(0x0202, 0x0B);
		write_cmos_sensor(0x0203, 0xF2);
		write_cmos_sensor(0x0224, 0x01);
		write_cmos_sensor(0x0225, 0xF4);
		write_cmos_sensor(0x0204, 0x00);
		write_cmos_sensor(0x0205, 0x00);
		write_cmos_sensor(0x020F, 0x00);
		write_cmos_sensor(0x0216, 0x00);
		write_cmos_sensor(0x0217, 0x00);
		write_cmos_sensor(0x0226, 0x01);
		write_cmos_sensor(0x0227, 0x00);
	}
#endif
}

static void normal_video_setting(kal_uint16 currefps, kal_uint8 pdaf_mode)
{
	LOG_INF("normal video currefps: %d\n", currefps);
#if MULTI_WRITE
	imx486_table_write_cmos_sensor(
		addr_data_pair_capture_30fps_imx486,
		sizeof(addr_data_pair_capture_30fps_imx486) /
		sizeof(kal_uint16));
#else
	capture_setting(currefps, pdaf_mode);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_imx486[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x08,
	0x0343, 0x30,
	0x0340, 0x04,
	0x0341, 0x6A,
	0x0344, 0x00,
	0x0345, 0x60,
	0x0346, 0x01,
	0x0347, 0xA8,
	0x0348, 0x0F,
	0x0349, 0x5F,
	0x034A, 0x0A,
	0x034B, 0x17,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x3130, 0x01,
	0x034C, 0x07,
	0x034D, 0x80,
	0x034E, 0x04,
	0x034F, 0x38,
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0x8F,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0x85,
	0x0310, 0x01,
	0x0700, 0x00,
	0x0701, 0xC8,
	0x0820, 0x0C,
	0x0821, 0x78,
	0x3100, 0x04,
	0x7036, 0x02,
	0x704E, 0x02,
	0x0202, 0x04,
	0x0203, 0x60,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0226, 0x01,
	0x0227, 0x00
};
#endif

static void hs_video_setting(void)
{
	LOG_INF("hs_video E\n");
#if MULTI_WRITE
	imx486_table_write_cmos_sensor(
		addr_data_pair_hs_video_imx486,
		sizeof(addr_data_pair_hs_video_imx486) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0342, 0x08);
	write_cmos_sensor(0x0343, 0x30);
	write_cmos_sensor(0x0340, 0x04);
	write_cmos_sensor(0x0341, 0x6A);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x60);
	write_cmos_sensor(0x0346, 0x01);
	write_cmos_sensor(0x0347, 0xA8);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0x5F);
	write_cmos_sensor(0x034A, 0x0A);
	write_cmos_sensor(0x034B, 0x17);
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x22);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3130, 0x01);
	write_cmos_sensor(0x034C, 0x07);
	write_cmos_sensor(0x034D, 0x80);
	write_cmos_sensor(0x034E, 0x04);
	write_cmos_sensor(0x034F, 0x38);
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0x8F);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x00);
	write_cmos_sensor(0x030F, 0x85);
	write_cmos_sensor(0x0310, 0x01);
	write_cmos_sensor(0x0700, 0x00);
	write_cmos_sensor(0x0701, 0xC8);
	write_cmos_sensor(0x0820, 0x0C);
	write_cmos_sensor(0x0821, 0x78);
	write_cmos_sensor(0x3100, 0x04);
	write_cmos_sensor(0x7036, 0x02);
	write_cmos_sensor(0x704E, 0x02);
	write_cmos_sensor(0x0202, 0x04);
	write_cmos_sensor(0x0203, 0x60);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0226, 0x01);
	write_cmos_sensor(0x0227, 0x00);
#endif
}

static void slim_video_setting(void)
{
	LOG_INF("slim video E\n");
	preview_setting();
}

static void custom1_setting(void)
{
	LOG_INF("%s E\n", __func__);
#if MULTI_WRITE
	imx486_table_write_cmos_sensor(
		addr_data_pair_preview_imx486,
		sizeof(addr_data_pair_preview_imx486) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0342, 0x08);
	write_cmos_sensor(0x0343, 0x30);
	write_cmos_sensor(0x0340, 0x0A);
	write_cmos_sensor(0x0341, 0x6E);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0xBF);
	write_cmos_sensor(0x034A, 0x0B);
	write_cmos_sensor(0x034B, 0xC7);
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x22);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3130, 0x01);
	write_cmos_sensor(0x034C, 0x07);
	write_cmos_sensor(0x034D, 0xE0);
	write_cmos_sensor(0x034E, 0x05);
	write_cmos_sensor(0x034F, 0xE4);
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0x54);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x02);
	write_cmos_sensor(0x030E, 0x00);
	write_cmos_sensor(0x030F, 0x2A);
	write_cmos_sensor(0x0310, 0x01);
	write_cmos_sensor(0x0700, 0x01);
	write_cmos_sensor(0x0701, 0x50);
	write_cmos_sensor(0x0820, 0x07);
	write_cmos_sensor(0x0821, 0xE0);
	write_cmos_sensor(0x3100, 0x06);
	write_cmos_sensor(0x7036, 0x02);
	write_cmos_sensor(0x704E, 0x02);
	write_cmos_sensor(0x0202, 0x0A);
	write_cmos_sensor(0x0203, 0x64);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0226, 0x01);
	write_cmos_sensor(0x0227, 0x00);
#endif
}

static void custom2_setting(void)
{
	LOG_INF("%s E\n", __func__);
#if MULTI_WRITE
	imx486_table_write_cmos_sensor(
		addr_data_pair_capture_30fps_imx486,
		sizeof(addr_data_pair_capture_30fps_imx486) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);
	write_cmos_sensor(0x0342, 0x10);
	write_cmos_sensor(0x0343, 0x60);
	write_cmos_sensor(0x0340, 0x0B);
	write_cmos_sensor(0x0341, 0xFC);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x0F);
	write_cmos_sensor(0x0349, 0xBF);
	write_cmos_sensor(0x034A, 0x0B);
	write_cmos_sensor(0x034B, 0xC7);
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	write_cmos_sensor(0x0902, 0x00);
	write_cmos_sensor(0x3130, 0x01);
	write_cmos_sensor(0x034C, 0x0F);
	write_cmos_sensor(0x034D, 0xC0);
	write_cmos_sensor(0x034E, 0x0B);
	write_cmos_sensor(0x034F, 0xC8);
	write_cmos_sensor(0x0301, 0x06);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0xC1);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x02);
	write_cmos_sensor(0x030E, 0x00);
	write_cmos_sensor(0x030F, 0x52);
	write_cmos_sensor(0x0310, 0x01);
	write_cmos_sensor(0x0700, 0x00);
	write_cmos_sensor(0x0701, 0x50);
	write_cmos_sensor(0x0820, 0x0F);
	write_cmos_sensor(0x0821, 0x60);
	write_cmos_sensor(0x3100, 0x06);
	write_cmos_sensor(0x7036, 0x00);
	write_cmos_sensor(0x704E, 0x00);
	write_cmos_sensor(0x0202, 0x0B);
	write_cmos_sensor(0x0203, 0xF2);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x0226, 0x01);
	write_cmos_sensor(0x0227, 0x00);
#endif
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
/* #define SLT_DEVINFO_CMM */
#ifdef SLT_DEVINFO_CMM
#include  <linux/dev_info.h>
static struct devinfo_struct *s_DEVINFO_ccm;	/* suppose 10 max lcm device */
#endif
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry_total = 1;
	kal_uint8 retry_cnt = retry_total;
#ifdef SLT_DEVINFO_CMM
	s_DEVINFO_ccm = kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);
	s_DEVINFO_ccm->device_type = "CCM";

	/* can change if got module id */
	s_DEVINFO_ccm->device_module = "PC0FB0002B";

	s_DEVINFO_ccm->device_vendor = "Sunrise";
	s_DEVINFO_ccm->device_ic = "IMX486";
	s_DEVINFO_ccm->device_version = "HI";
	s_DEVINFO_ccm->device_info = "200W";
#endif
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
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
#ifdef SLT_DEVINFO_CMM
				s_DEVINFO_ccm->device_used = DEVINFO_USED;
				devinfo_check_add_device(s_DEVINFO_ccm);
#endif
				return ERROR_NONE;
			}

		      LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);

			retry_cnt--;
		} while (retry_cnt > 0);
		i++;
		retry_cnt = retry_total;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {

	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
#ifdef SLT_DEVINFO_CMM
		s_DEVINFO_ccm->device_used = DEVINFO_UNUSED;
		devinfo_check_add_device(s_DEVINFO_ccm);
#endif
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
	/* const kal_uint8 i2c_addr[] = {
	 * IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	 */
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_INF("IMX486,MIPI 4LANE\n");
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
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}

	if (imgsensor_info.sensor_id != sensor_id) {
		LOG_INF("Open sensor id fail id: 0x%x\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0xe000;	/* 0x100; */
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

	return ERROR_NONE;
}				/*      open  */



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
	return ERROR_NONE;
}				/*      preview   */

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
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
#if 0
	else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
#endif
	else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
	  "current_fps %d fps is not support, so use cap's setting: %d fps!\n",
		imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps, imgsensor.pdaf_mode);

	mdelay(100);
	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("ihdr enable :%d\n", imgsensor.hdr_mode);
	normal_video_setting(imgsensor.current_fps, imgsensor.pdaf_mode);
	return ERROR_NONE;
}				/*      normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 600; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 1200; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();

	return ERROR_NONE;
}				/*      slim_video       */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	return ERROR_NONE;
}			   /*	   stereo	 */

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
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

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;
/*add end*/

	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

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

	sensor_info->SettleDelayMode =
		imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;

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

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->TEMPERATURE_SUPPORT = 0;

	sensor_info->PDAF_Support = PDAF_SUPPORT_RAW;

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
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*      get_info  */


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
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
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
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
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

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

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
		if (imgsensor.frame_length > imgsensor.shutter)
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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
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
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
			    "current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			    framerate,
			    imgsensor_info.cap.max_framerate / 10);

		frame_length = imgsensor_info.cap.pclk /
			framerate * 10 / imgsensor_info.cap.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.cap.framelength)
		? (frame_length - imgsensor_info.cap.framelength) : 0;

		imgsensor.frame_length =
		    imgsensor_info.cap.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
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
		if (imgsensor.frame_length > imgsensor.shutter)
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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk
			/ framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.custom1.framelength)
		? (frame_length - imgsensor_info.custom1.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.custom1.framelength +
			imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk
			/ framerate * 10 / imgsensor_info.custom2.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.custom2.framelength)
		? (frame_length - imgsensor_info.custom2.framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.custom2.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
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
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}


static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("%s enable: %d\n", __func__, enable);

	if (enable)
		write_cmos_sensor(0x0601, 0x02);
	else
		write_cmos_sensor(0x0601, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	/*TEMP_SEN_CTL */
	write_cmos_sensor(0x0138, 0x01);
	temperature = read_cmos_sensor(0x013a);

	if (temperature >= 0x0 && temperature <= 0x4F)
		temperature_convert = temperature;
	else if (temperature >= 0x50 && temperature <= 0x7F)
		temperature_convert = 80;
	else if (temperature >= 0x80 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (INT8) temperature;

	LOG_INF("temp_c(%d), read_reg(%d)\n",
		temperature_convert, temperature);
	return 20;
	//return temperature_convert;
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
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	/* unsigned long long *feature_return_para =
	 * (unsigned long long *) feature_para;
	 */

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	//struct SENSOR_VC_INFO_STRUCT *pvcinfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/* LOG_INF("feature_id = %d\n", feature_id); */

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
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
					      *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;

	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		#if 0
		read_imx486_pdaf((kal_uint16) (*feature_data),
				 (char *)(uintptr_t) (*(feature_data + 1)),
				 (kal_uint32) (*(feature_data + 2)));
		#endif
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		LOG_INF("SENSOR_FEATURE_SET_TEST_PATTERN\n");
		set_test_pattern_mode((BOOL) (*feature_data));
		break;

	/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		/* HDR mODE : 0: disable HDR, 1:IHDR, 2:HDR, 9:ZHDR */
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
		LOG_INF("ihdr enable :%d\n", imgsensor.hdr_mode);
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[6],
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
#if 1
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%llu\n",
			*feature_data);

		PDAFinfo =
	      (struct SET_PD_BLOCK_INFO_T *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(
			(void *)PDAFinfo,
			(void *)&imgsensor_pd_info,
			sizeof(struct SET_PD_BLOCK_INFO_T));
			break;

		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			break;
		}
		break;
#endif
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

		ihdr_write_shutter_gain((UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		break;
		/*PDAF CMD */
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF(
		    "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n",
			*feature_data);

	/* PDAF capacity enable or not, 2p8 only full size support PDAF */
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_INF("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;

	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_GET_PDAF_TYPE:
		break;
	case SENSOR_FEATURE_SET_PDAF_TYPE:
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
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX486_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      IMX486_MIPI_RAW_SensorInit      */
