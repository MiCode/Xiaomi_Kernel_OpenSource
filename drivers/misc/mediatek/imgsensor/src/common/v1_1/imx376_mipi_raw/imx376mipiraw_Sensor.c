// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX376mipi_Sensor.c
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
#include <linux/atomic.h>
/* #include <asm/system.h> */
/* #include <linux/xlog.h> */
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx376mipiraw_Sensor.h"


#define PFX "IMX376_camera_sensor"

#define USE_REMOSAIC 1

/* Add by LiuBin for register device info at 20160616 */
#define DEVICE_VERSION_IMX376     "imx376"
/* extern
 * void register_imgsensor_deviceinfo(char *name, char *version, u8 module_id);
 */

/* static uint8_t deviceInfo_register_value = 0x00; */

/* #define LOG_WRN(format, args...)
 * xlog_printk(ANDROID_LOG_WARN ,PFX, "[%S] " format, __FUNCTION__, ##args)
 */

/* #defineLOG_INF(format, args...)
 * xlog_printk(ANDROID_LOG_INFO ,PFX, "[%s] " format, __FUNCTION__, ##args)
 */

/* #define LOG_DBG(format, args...)
 * xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S] " format, __FUNCTION__, ##args)
 */
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX376_SENSOR_ID,

	/*zhaozhengtao 2016/02/19,modify for different module*/
	.module_id = 0x01,  /* 0x01 Sunny,0x05 QTEK */

	.checksum_value = 0xffb1ec31,

	.pre = {
		.pclk = 420000000,
		.linelength = 6976,
		.framelength = 2004,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1940,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 250000000,
		.max_framerate = 300,
	},
	.cap = {
#if USE_REMOSAIC
		.pclk = 801600000,
		.linelength = 6208,
		.framelength = 5380,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5184,
		.grabwindow_height = 3880,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 720000000,
		.max_framerate = 240,

#else
		.pclk = 420000000,
		.linelength = 6976,
		.framelength = 2004,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1940,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
#endif
	},
	.normal_video = {
		.pclk = 420000000,
		.linelength = 6976,
		.framelength = 2004,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1940,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 250000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 420000000,
		.linelength = 6976,
		.framelength = 2004,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1940,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 250000000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 420000000,
		.linelength = 6976,
		.framelength = 2004,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1940,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 250000000,
		.max_framerate = 300,
	},
	.margin = 5,
	.min_shutter = 1,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  /* 1, support; 0,not support */
	.ihdr_le_firstline = 0,  /* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	  /* support sensor mode num */

	.cap_delay_frame = 3,/* 3 guanjd modify */
	.pre_delay_frame = 2,/* 3 guanjd modify */
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	/*zhengjiang.zhu@EXP CameraDrv, 2017/03/03 Increase ISP drive ability */
	.isp_driving_current = ISP_DRIVING_8MA,  /* 2ma */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0, /* 0,MIPI_SETTLEDELAY_AUTO */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_R,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20, 0x21, 0xff},
	.i2c_speed = 320, /* i2c read/write speed */
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,		/* mirrorflip information */
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT,
	 * Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20,

};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
{ 5184, 3880, 0, 0, 5184, 3880, 2592, 1940, 0000, 0000, 2592, 1940,
		0,  0,  2592,  1940},/*Preview*/
#if USE_REMOSAIC
{ 5184, 3880, 0, 0, 5184, 3880, 5184, 3880, 0000, 0000, 5184, 3880,
		0, 0, 5184, 3880}, /* remosic*/
#else
{ 2592, 1940, 0, 0, 2592, 1940, 2592, 1940, 0000, 0000, 2592, 1940,
		0, 0, 2592, 1940}, /* capture*/
#endif
{ 5184, 3880, 0, 0, 5184, 3880, 2592, 1940, 0000, 0000, 2592, 1940,
		0, 0, 2592, 1940}, /* video */
{ 5184, 3880, 0, 0, 5184, 3880, 2592, 1940, 0000, 0000, 2592, 1940,
		0, 0, 2592, 1940}, /*hs video*/
{ 5184, 3880, 0, 0, 5184, 3880, 2592, 1940, 0000, 0000, 2592, 1940,
		0, 0, 2592, 1940}, /* slim video*/
};

#if 0
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	iReadReg((u16) addr, (u8 *)&get_byte, imgsensor.i2c_write_id);
	return get_byte;
}

#define write_cmos_sensor(addr, para) \
	iWriteReg((u16) addr, (u32) para, 1,  imgsensor.i2c_write_id)
#endif
#define RWB_ID_OFFSET 0x0F73

/*zhaozhengtao 2016/02/19,modify for different module*/
#define MODULE_ID_OFFSET 0x0000

#define EEPROM_READ_ID  0xA2
#define EEPROM_WRITE_ID   0xA1
#if 0
static kal_uint16 is_RWB_sensor(void)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(RWB_ID_OFFSET >> 8),
			(char)(RWB_ID_OFFSET & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, EEPROM_READ_ID);
	return get_byte;

}
#endif

/*zhaozhengtao 2016/02/19,modify for different module*/
static kal_uint16 read_module_id(void)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(MODULE_ID_OFFSET >> 8),
				(char)(MODULE_ID_OFFSET & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA2/*EEPROM_READ_ID*/);
	return get_byte;

}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
	/* Add this func to set i2c speed by each sensor */
	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
				(char)(para >> 8), (char)(para & 0xFF)};

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
	/* Add this func to set i2c speed by each sensor */
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
	/* Add this func to set i2c speed by each sensor */
	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {(char)(addr >> 8),
				(char)(addr & 0xFF),
				(char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* return; //for test */
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

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
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
				imgsensor.line_length * 10 /
				imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340,
				imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341,
				imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

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




static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain;

	reg_gain = 1024 - (1024*64)/gain;

	LOG_INF("imx376 gain =%d, reg_gain =%d\n", gain, reg_gain);

	return reg_gain;
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

    /* gain=1024;//for test */
    /* return; //for test */

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

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	return gain;
}	/*	set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
	write_cmos_sensor_8(0x0101, itemp);
	break;

	case IMAGE_V_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x02);
	break;

	case IMAGE_H_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x01);
	break;

	case IMAGE_HV_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x03);
	break;
	}
}

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
#if 0
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
#endif

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3

#endif
static kal_uint16 imx376_table_write_cmos_sensor(
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
/* Write when remain buffer size is less than 3 bytes or reach end of data */
		if ((I2C_BUFFER_LEN - tosend) < 3 ||
			len == IDX || addr != addr_last) {
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

kal_uint16 addr_data_pair_init_imx376[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x3C7D, 0x28,
	0x3C7E, 0x02,
	0x3C7F, 0x06,
	0x3F02, 0x02,
	0x3F22, 0x01,
	0x3F7F, 0x01,
	0x4421, 0x04,
	0x4430, 0x05,
	0x4431, 0xDC,
	0x5222, 0x02,
	0x56B7, 0x74,
	0x6204, 0xC6,
	0x620E, 0x27,
	0x6210, 0x69,
	0x6211, 0xD6,
	0x6213, 0x01,
	0x6215, 0x5A,
	0x6216, 0x75,
	0x6218, 0x5A,
	0x6219, 0x75,
	0x6220, 0x06,
	0x6222, 0x0C,
	0x6225, 0x19,
	0x6228, 0x32,
	0x6229, 0x70,
	0x622B, 0x64,
	0x622E, 0xB0,
	0x6231, 0x71,
	0x6234, 0x06,
	0x6236, 0x46,
	0x6237, 0x46,
	0x6239, 0x0C,
	0x623C, 0x19,
	0x623F, 0x32,
	0x6240, 0x71,
	0x6242, 0x64,
	0x6243, 0x44,
	0x6245, 0xB0,
	0x6246, 0xA8,
	0x6248, 0x71,
	0x624B, 0x06,
	0x624D, 0x46,
	0x625C, 0xC9,
	0x625F, 0x92,
	0x6262, 0x26,
	0x6264, 0x46,
	0x6265, 0x46,
	0x6267, 0x0C,
	0x626A, 0x19,
	0x626D, 0x32,
	0x626E, 0x72,
	0x6270, 0x64,
	0x6271, 0x68,
	0x6273, 0xC8,
	0x6276, 0x91,
	0x6279, 0x27,
	0x627B, 0x46,
	0x627C, 0x55,
	0x627F, 0x95,
	0x6282, 0x84,
	0x6283, 0x40,
	0x6284, 0x00,
	0x6285, 0x00,
	0x6286, 0x08,
	0x6287, 0xC0,
	0x6288, 0x00,
	0x6289, 0x00,
	0x628A, 0x1B,
	0x628B, 0x80,
	0x628C, 0x20,
	0x628E, 0x35,
	0x628F, 0x00,
	0x6290, 0x50,
	0x6291, 0x00,
	0x6292, 0x14,
	0x6293, 0x00,
	0x6294, 0x00,
	0x6296, 0x54,
	0x6297, 0x00,
	0x6298, 0x00,
	0x6299, 0x01,
	0x629A, 0x10,
	0x629B, 0x01,
	0x629C, 0x00,
	0x629D, 0x03,
	0x629E, 0x50,
	0x629F, 0x05,
	0x62A0, 0x00,
	0x62B1, 0x00,
	0x62B2, 0x00,
	0x62B3, 0x00,
	0x62B5, 0x00,
	0x62B6, 0x00,
	0x62B7, 0x00,
	0x62B8, 0x00,
	0x62B9, 0x00,
	0x62BA, 0x00,
	0x62BB, 0x00,
	0x62BC, 0x00,
	0x62BD, 0x00,
	0x62BE, 0x00,
	0x62BF, 0x00,
	0x62D0, 0x0C,
	0x62D1, 0x00,
	0x62D2, 0x00,
	0x62D4, 0x40,
	0x62D5, 0x00,
	0x62D6, 0x00,
	0x62D7, 0x00,
	0x62D8, 0xD8,
	0x62D9, 0x00,
	0x62DA, 0x00,
	0x62DB, 0x02,
	0x62DC, 0xB0,
	0x62DD, 0x03,
	0x62DE, 0x00,
	0x62EF, 0x14,
	0x62F0, 0x00,
	0x62F1, 0x00,
	0x62F3, 0x58,
	0x62F4, 0x00,
	0x62F5, 0x00,
	0x62F6, 0x01,
	0x62F7, 0x20,
	0x62F8, 0x00,
	0x62F9, 0x00,
	0x62FA, 0x03,
	0x62FB, 0x80,
	0x62FC, 0x00,
	0x62FD, 0x00,
	0x62FE, 0x04,
	0x62FF, 0x60,
	0x6300, 0x04,
	0x6301, 0x00,
	0x6302, 0x09,
	0x6303, 0x00,
	0x6304, 0x0C,
	0x6305, 0x00,
	0x6306, 0x1B,
	0x6307, 0x80,
	0x6308, 0x30,
	0x630A, 0x38,
	0x630B, 0x00,
	0x630C, 0x60,
	0x630E, 0x14,
	0x630F, 0x00,
	0x6310, 0x00,
	0x6312, 0x58,
	0x6313, 0x00,
	0x6314, 0x00,
	0x6315, 0x01,
	0x6316, 0x18,
	0x6317, 0x01,
	0x6318, 0x80,
	0x6319, 0x03,
	0x631A, 0x60,
	0x631B, 0x06,
	0x631C, 0x00,
	0x632D, 0x0E,
	0x632E, 0x00,
	0x632F, 0x00,
	0x6331, 0x44,
	0x6332, 0x00,
	0x6333, 0x00,
	0x6334, 0x00,
	0x6335, 0xE8,
	0x6336, 0x00,
	0x6337, 0x00,
	0x6338, 0x02,
	0x6339, 0xF0,
	0x633A, 0x00,
	0x633B, 0x00,
	0x634C, 0x0C,
	0x634D, 0x00,
	0x634E, 0x00,
	0x6350, 0x40,
	0x6351, 0x00,
	0x6352, 0x00,
	0x6353, 0x00,
	0x6354, 0xD8,
	0x6355, 0x00,
	0x6356, 0x00,
	0x6357, 0x02,
	0x6358, 0xB0,
	0x6359, 0x04,
	0x635A, 0x00,
	0x636B, 0x00,
	0x636C, 0x00,
	0x636D, 0x00,
	0x636F, 0x00,
	0x6370, 0x00,
	0x6371, 0x00,
	0x6372, 0x00,
	0x6373, 0x00,
	0x6374, 0x00,
	0x6375, 0x00,
	0x6376, 0x00,
	0x6377, 0x00,
	0x6378, 0x00,
	0x6379, 0x00,
	0x637A, 0x13,
	0x637B, 0xD4,
	0x6388, 0x22,
	0x6389, 0x82,
	0x638A, 0xC8,
	0x639D, 0x20,
	0x7BA0, 0x01,
	0x7BA9, 0x00,
	0x7BAA, 0x01,
	0x7BAD, 0x00,
	0x9002, 0x00,
	0x9003, 0x00,
	0x9004, 0x0D,
	0x9006, 0x01,
	0x9200, 0x93,
	0x9201, 0x85,
	0x9202, 0x93,
	0x9203, 0x87,
	0x9204, 0x93,
	0x9205, 0x8D,
	0x9206, 0x93,
	0x9207, 0x8F,
	0x9208, 0x62,
	0x9209, 0x2C,
	0x920A, 0x62,
	0x920B, 0x2F,
	0x920C, 0x6A,
	0x920D, 0x23,
	0x920E, 0x71,
	0x920F, 0x08,
	0x9210, 0x71,
	0x9211, 0x09,
	0x9212, 0x71,
	0x9213, 0x0B,
	0x9214, 0x6A,
	0x9215, 0x0F,
	0x9216, 0x71,
	0x9217, 0x07,
	0x9218, 0x71,
	0x9219, 0x03,
	0x935D, 0x01,
	0x9389, 0x05,
	0x938B, 0x05,
	0x9391, 0x05,
	0x9393, 0x05,
	0x9395, 0x65,
	0x9397, 0x5A,
	0x9399, 0x05,
	0x939B, 0x05,
	0x939D, 0x05,
	0x939F, 0x05,
	0x93A1, 0x05,
	0x93A3, 0x05,
	0xB3F1, 0x80,
	0xB3F2, 0x0E,
	0xBC40, 0x03,
	0xBC82, 0x07,
	0xBC83, 0xB0,
	0xBC84, 0x0D,
	0xBC85, 0x08,
	0xE0A6, 0x0A,
	0xAA3F, 0x04,
	0xAA41, 0x03,
	0xAA43, 0x02,
	0xAA5D, 0x05,
	0xAA5F, 0x03,
	0xAA61, 0x02,
	0xAACF, 0x04,
	0xAAD1, 0x03,
	0xAAD3, 0x02,
	0xAAED, 0x05,
	0xAAEF, 0x03,
	0xAAF1, 0x02,
	0xB6D9, 0x00,
};

static void sensor_init(void)
{
	imx376_table_write_cmos_sensor(addr_data_pair_init_imx376,
		sizeof(addr_data_pair_init_imx376)/sizeof(kal_uint16));
}	/*	sensor_init  */

kal_uint16 addr_data_pair_preview_imx376[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x1B,
	0x0343, 0x40,
	0x0340, 0x07,
	0x0341, 0xD4,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x14,
	0x0349, 0x3F,
	0x034A, 0x0F,
	0x034B, 0x27,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3F4D, 0x81,
	0x3F4C, 0x81,
	0x4254, 0x7F,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0A,
	0x040D, 0x20,
	0x040E, 0x07,
	0x040F, 0x94,
	0x034C, 0x0A,
	0x034D, 0x20,
	0x034E, 0x07,
	0x034F, 0x94,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x5E,
	0x030B, 0x02,
	0x030D, 0x0C,
	0x030E, 0x02,
	0x030F, 0x71,
	0x0310, 0x01,
	0x0820, 0x09,
	0x0821, 0xC4,
	0x0822, 0x00,
	0x0823, 0x00,
	0xBC41, 0x01,
	0x0106, 0x00,
	0x0B00, 0x00,
	0x0B05, 0x01,
	0x0B06, 0x01,
	0x3230, 0x00,
	0x3602, 0x01,
	0x3C00, 0x74,
	0x3C01, 0x5F,
	0x3C02, 0x73,
	0x3C03, 0x64,
	0x3C04, 0x54,
	0x3C05, 0xA8,
	0x3C06, 0xE4,
	0x3C07, 0x00,
	0x3C08, 0x00,
	0x3C09, 0x01,
	0x3C0A, 0x14,
	0x3C0B, 0x01,
	0x3C0C, 0x00,
	0x3F14, 0x00,
	0x3F17, 0x00,
	0x3F3C, 0x00,
	0x3F78, 0x04,
	0x3F79, 0x74,
	0x3F7A, 0x04,
	0x3F7B, 0x24,
	0x562B, 0x0A,
	0x562D, 0x0C,
	0x5617, 0x0A,
	0x9104, 0x04,
	0x0202, 0x07,
	0x0203, 0xC0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3614, 0x00,
	0x3616, 0x0D,
	0x3617, 0x56,
	0xB612, 0x2C,
	0xB613, 0x2C,
	0xB614, 0x1C,
	0xB615, 0x1C,
	0xB616, 0x06,
	0xB617, 0x06,
	0xB618, 0x20,
	0xB619, 0x20,
	0xB61A, 0x0C,
	0xB61B, 0x0C,
	0xB61C, 0x06,
	0xB61D, 0x06,
	0xB666, 0x39,
	0xB667, 0x39,
	0xB668, 0x39,
	0xB669, 0x39,
	0xB66A, 0x13,
	0xB66B, 0x13,
	0xB66C, 0x20,
	0xB66D, 0x20,
	0xB66E, 0x20,
	0xB66F, 0x20,
	0xB670, 0x10,
	0xB671, 0x10,
	0x3900, 0x00,
	0x3901, 0x00,
	0x3237, 0x00,
	0x30AC, 0x00,
};

static void preview_setting(void)
{
	write_cmos_sensor_8(0x0100, 0x00);
	mdelay(100);

	imx376_table_write_cmos_sensor(addr_data_pair_preview_imx376,
		sizeof(addr_data_pair_preview_imx376) / sizeof(kal_uint16));
}	/*	preview_setting  */
/* ==================================================== */
/* 3P3SP EVT0 */
/* Full resolution */
/* x_output_size: 2304 */
/* y_output_size: 1728 */
/* frame_rate: 30.000 */
/* output_format: RAW10 */
/* output_interface: MIPI */
/* output_lanes: 4 */
/* output_clock_mhz: 720.00 */
/* system_clock_mhz: 280.00 */
/* input_clock_mhz: 24.00 */
/*  */
/* $Rev$: Revision 0.00 */
/* $Date$: 20151201 */
/* ==================================================== */
/* $MV1[MCLK:24,Width:2304,Height:1728,Format:MIPI_RAW10,mipi_lane:4
 * ,mipi_datarate:720,pvi_pclk_inverse:0]
 */

/* Pll Setting - VCO = 280Mhz */

kal_uint16 addr_data_pair_capture_imx376[] = {
#if USE_REMOSAIC
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x18,
	0x0343, 0x40,
	0x0340, 0x15,
	0x0341, 0x04,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x14,
	0x0349, 0x3F,
	0x034A, 0x0F,
	0x034B, 0x27,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x3F4D, 0x01,
	0x3F4C, 0x01,
	0x4254, 0x7F,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x14,
	0x040D, 0x40,
	0x040E, 0x0F,
	0x040F, 0x28,
	0x034C, 0x14,
	0x034D, 0x40,
	0x034E, 0x0F,
	0x034F, 0x28,
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x4E,
	0x030B, 0x01,
	0x030D, 0x0C,
	0x030E, 0x03,
	0x030F, 0x84,
	0x0310, 0x01,
	0x0820, 0x1C,
	0x0821, 0x20,
	0x0822, 0x00,
	0x0823, 0x00,
	0xBC41, 0x01,
	0x0106, 0x00,
	0x0B00, 0x00,
	0x0B05, 0x00,
	0x0B06, 0x00,
	0x3230, 0x00,
	0x3602, 0x00,
	0x3C00, 0x5B,
	0x3C01, 0x4A,
	0x3C02, 0x73,
	0x3C03, 0x64,
	0x3C04, 0x34,
	0x3C05, 0x88,
	0x3C06, 0xD2,
	0x3C07, 0x00,
	0x3C08, 0x00,
	0x3C09, 0x01,
	0x3C0A, 0x14,
	0x3C0B, 0x01,
	0x3C0C, 0x01,
	0x3F14, 0x01,
	0x3F17, 0x00,
	0x3F3C, 0x01,
	0x3F78, 0x01,
	0x3F79, 0xF4,
	0x3F7A, 0x02,
	0x3F7B, 0xDA,
	0x562B, 0x32,
	0x562D, 0x34,
	0x5617, 0x32,
	0x9104, 0x04,
	0x0202, 0x14,
	0x0203, 0xF0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3614, 0x00,
	0x3616, 0x0D,
	0x3617, 0x56,
	0xB612, 0x2C,
	0xB613, 0x2C,
	0xB614, 0x1C,
	0xB615, 0x1C,
	0xB616, 0x06,
	0xB617, 0x06,
	0xB618, 0x20,
	0xB619, 0x20,
	0xB61A, 0x0C,
	0xB61B, 0x0C,
	0xB61C, 0x06,
	0xB61D, 0x06,
	0xB666, 0x39,
	0xB667, 0x39,
	0xB668, 0x39,
	0xB669, 0x39,
	0xB66A, 0x13,
	0xB66B, 0x13,
	0xB66C, 0x20,
	0xB66D, 0x20,
	0xB66E, 0x20,
	0xB66F, 0x20,
	0xB670, 0x10,
	0xB671, 0x10,
	0x3900, 0x00,
	0x3901, 0x00,
	0x3237, 0x00,
	0x30AC, 0x00,
#else
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x1B,
	0x0343, 0x40,
	0x0340, 0x07,
	0x0341, 0xD4,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x14,
	0x0349, 0x3F,
	0x034A, 0x0F,
	0x034B, 0x27,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3F4D, 0x81,
	0x3F4C, 0x81,
	0x4254, 0x7F,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0A,
	0x040D, 0x20,
	0x040E, 0x07,
	0x040F, 0x94,
	0x034C, 0x0A,
	0x034D, 0x20,
	0x034E, 0x07,
	0x034F, 0x94,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x5E,
	0x030B, 0x02,
	0x030D, 0x0C,
	0x030E, 0x02,
	0x030F, 0x71,
	0x0310, 0x01,
	0x0820, 0x09,
	0x0821, 0xC4,
	0x0822, 0x00,
	0x0823, 0x00,
	0xBC41, 0x01,
	0x0106, 0x00,
	0x0B00, 0x00,
	0x0B05, 0x01,
	0x0B06, 0x01,
	0x3230, 0x00,
	0x3602, 0x01,
	0x3C00, 0x74,
	0x3C01, 0x5F,
	0x3C02, 0x73,
	0x3C03, 0x64,
	0x3C04, 0x54,
	0x3C05, 0xA8,
	0x3C06, 0xE4,
	0x3C07, 0x00,
	0x3C08, 0x00,
	0x3C09, 0x01,
	0x3C0A, 0x14,
	0x3C0B, 0x01,
	0x3C0C, 0x00,
	0x3F14, 0x00,
	0x3F17, 0x00,
	0x3F3C, 0x00,
	0x3F78, 0x04,
	0x3F79, 0x74,
	0x3F7A, 0x04,
	0x3F7B, 0x24,
	0x562B, 0x0A,
	0x562D, 0x0C,
	0x5617, 0x0A,
	0x9104, 0x04,
	0x0202, 0x07,
	0x0203, 0xC0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3614, 0x00,
	0x3616, 0x0D,
	0x3617, 0x56,
	0xB612, 0x2C,
	0xB613, 0x2C,
	0xB614, 0x1C,
	0xB615, 0x1C,
	0xB616, 0x06,
	0xB617, 0x06,
	0xB618, 0x20,
	0xB619, 0x20,
	0xB61A, 0x0C,
	0xB61B, 0x0C,
	0xB61C, 0x06,
	0xB61D, 0x06,
	0xB666, 0x39,
	0xB667, 0x39,
	0xB668, 0x39,
	0xB669, 0x39,
	0xB66A, 0x13,
	0xB66B, 0x13,
	0xB66C, 0x20,
	0xB66D, 0x20,
	0xB66E, 0x20,
	0xB66F, 0x20,
	0xB670, 0x10,
	0xB671, 0x10,
	0x3900, 0x00,
	0x3901, 0x00,
	0x3237, 0x00,
	0x30AC, 0x00,
#endif
};

static void capture_setting(kal_uint16 currefps, kal_bool stream_on)
{
	LOG_INF("E! currefps:%d\n", currefps);
	/* full size 29.76fps */
	/* capture setting */
	write_cmos_sensor_8(0x0100, 0x00);
	mdelay(100);

	imx376_table_write_cmos_sensor(addr_data_pair_capture_imx376,
		sizeof(addr_data_pair_capture_imx376) / sizeof(kal_uint16));
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E\n");
	preview_setting();	/* Tower modify 20160214 */
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();  /* Tower modify 20160214 */
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
			*sensor_id = ((read_cmos_sensor_8(0x0016) << 8) |
						read_cmos_sensor_8(0x0017));
			LOG_INF(
				"read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
				read_cmos_sensor_8(0x0000),
				read_cmos_sensor_8(0x0001),
				read_cmos_sensor(0x0000));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);

			/*zhaozhengtao 2016/02/19,modify for different module*/
				imgsensor_info.module_id = read_module_id();

				LOG_INF("IMX376_module_id=%d\n",
					imgsensor_info.module_id);
				return ERROR_NONE;
			}
			LOG_INF(
				"Read sensor id fail, i2c_write_id: 0x%x  sensor_id: 0x%x\n",
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
	kal_uint16 sensor_id = 0;

	LOG_INF("PLATFORM:MT6595,MIPI 2LANE\n");
	LOG_INF(
	"preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane;\n");
	LOG_INF("capture 5M@30fps,864Mbps/lane\n");

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id =
				((read_cmos_sensor_8(0x0016) << 8) |
				read_cmos_sensor_8(0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id:0x%x, sensor id:0x%x\n",
			imgsensor.i2c_write_id, sensor_id);
			break;
		}
			LOG_INF(
			"Read sensor id fail, id: 0x%x sensor_id=0x%x\n",
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
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
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
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);

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
	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}  else if (imgsensor.current_fps ==
			imgsensor_info.cap2.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps, 1);
	set_mirror_flip(imgsensor.mirror);

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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);
	/* preview_setting(); */
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT
				 *sensor_resolution)
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


	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;
	/* zhaozhengtao add 20160215 */
	sensor_resolution->SensorCustom1Width =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorCustom2Width =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorCustom3Width =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorCustom4Width =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorCustom5Width =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.cap.grabwindow_height;

	return ERROR_NONE;
}	/*	get_resolution	*/

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
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

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

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
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
	sensor_info->PDAF_Support = 0;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
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
}	/*	get_info  */


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
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
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
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.normal_video.framelength)
		  ? (frame_length - imgsensor_info.normal_video.framelength)
		  : 0;
		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
		    imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk /
				framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap1.framelength) ?
			    (frame_length - imgsensor_info.cap1.framelength) :
			    0;
			imgsensor.frame_length =
				imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (imgsensor.current_fps ==
		    imgsensor_info.cap2.max_framerate) {
			frame_length = imgsensor_info.cap2.pclk / framerate * 10
				/ imgsensor_info.cap2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap2.framelength) ?
			    (frame_length - imgsensor_info.cap2.framelength) :
			    0;
			imgsensor.frame_length =
				imgsensor_info.cap2.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
				LOG_INF(
				    "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				    framerate,
				    imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk /
					framerate * 10 /
					imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (
			  frame_length - imgsensor_info.cap.framelength)
			: 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length =
				imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		(frame_length > imgsensor_info.hs_video.framelength)
		  ? (frame_length - imgsensor_info.hs_video.framelength)
		  : 0;
		imgsensor.frame_length =
		  imgsensor_info.hs_video.framelength
		  + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		(frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.framelength)
		: 0;
		imgsensor.frame_length =
		  imgsensor_info.slim_video.framelength +
		  imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:  /* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.pre.framelength)
		  ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
		  imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF(
			"error scenario_id = %d, we use preview scenario\n",
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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
	/* 0x5E00[8]: 1 enable,  0 disable */
	/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor(0x0601, 0x0002);
	} else {
	/* 0x5E00[8]: 1 enable,  0 disable */
	/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor(0x0601, 0x0000);
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
		write_cmos_sensor_8(0x0100, 0x01);
	else
		write_cmos_sensor_8(0x0100, 0x00);

	mdelay(10);
	return ERROR_NONE;
}

#define FOUR_CELL_SIZE 560
static void read_4cell_from_eeprom(char *data)
{
	int i = 0;
	int addr = 0x1400;/*Start of 4 cell data*/
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	data[0] = (FOUR_CELL_SIZE & 0xff);/*Low*/
	data[1] = ((FOUR_CELL_SIZE >> 8) & 0xff);/*High*/

	for (i = 2; i < (FOUR_CELL_SIZE + 2); i++) {
		pu_send_cmd[0] = (char)(addr >> 8);
		pu_send_cmd[1] = (char)(addr & 0xFF);
		iReadRegI2C(pu_send_cmd, 2, &data[i], 1, EEPROM_READ_ID);
		addr++;
	}
}



static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
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
			LOG_INF(
				"feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
				imgsensor.pclk, imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_SET_ESHUTTER:
	    set_shutter(*feature_data);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
		     /* night_mode((BOOL) *feature_data); */
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
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
			break;
		case SENSOR_FEATURE_GET_PDAF_DATA:
			LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
			/* read_3P3_eeprom((kal_uint16 )(*feature_data),
			 * (char*)(uintptr_t)(*(feature_data+1)),
			 * (kal_uint32)(*(feature_data+2)));
			 */
			break;
		case SENSOR_FEATURE_GET_4CELL_DATA:
		{
			/*get 4 cell data from eeprom*/
			int type = (kal_uint16)(*feature_data);
			char *data = (char *)(uintptr_t)(*(feature_data+1));

			memset(data, 0, FOUR_CELL_SIZE);

			if (type == FOUR_CELL_CAL_TYPE_GAIN_TBL) {
				read_4cell_from_eeprom(data);
				LOG_INF(
				    "read Cross Talk = %02x %02x %02x %02x %02x %02x\n",
				    (UINT16)data[0], (UINT16)data[1],
				    (UINT16)data[2], (UINT16)data[3],
				    (UINT16)data[4], (UINT16)data[5]);
			}
			break;
		}

		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
			/* for factory mode auto testing */
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_SET_FRAMERATE:
			LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = *feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_SET_HDR:
			LOG_INF("ihdr enable :%d\n", *feature_data_32);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.ihdr_mode = (UINT8)*feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			/* LOG_INF(
			 * "SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			 * (UINT32)*feature_data);
			 */
			wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
				(uintptr_t) (*(feature_data + 1));

			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)wininfo,
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
		case SENSOR_FEATURE_GET_PDAF_INFO:
			/* LOG_INF(
			 * "SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			 *	*feature_data);
			 */

			PDAFinfo =
			(struct SET_PD_BLOCK_INFO_T *) (uintptr_t)
				(*(feature_data + 1));

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				/* memcpy((void *)PDAFinfo,
				 *	(void *)&imgsensor_pd_info,
				 *	sizeof(SET_PD_BLOCK_INFO_T));
				 */
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				break;
			}
			break;
		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
			/* LOG_INF( */
			/* "GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n" */
			/* , *feature_data); */

			/* PDAF capacity enable or not,
			 * 2p8 only full size support PDAF
			 */
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				/* video & capture use same setting */
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			}
			break;
		case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate =
				  imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate =
				  imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;

		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
			LOG_INF(
			"SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
			/* ihdr_write_shutter_gain((UINT16)*feature_data,
			 *(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			 */
			break;
		case SENSOR_FEATURE_SET_AWB_GAIN:
			break;
		case SENSOR_FEATURE_SET_HDR_SHUTTER:
		    LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
			/* ihdr_write_shutter((UINT16)*feature_data,
			 *(UINT16)*(feature_data+1));
			 */
			break;
		case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
			streaming_control(KAL_FALSE);
			break;
		case SENSOR_FEATURE_SET_STREAMING_RESUME:
			LOG_INF(
			"SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
			if (*feature_data != 0)
				set_shutter(*feature_data);
			streaming_control(KAL_TRUE);
			break;
		default:
			break;
	}

	return ERROR_NONE;
}	/*	feature_control()  */


static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

/* kin0603 */
UINT32 IMX376_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	OV5693_MIPI_RAW_SensorInit	*/
