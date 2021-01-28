// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *     GC8034mipi_Sensor.c
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
 *-----------------------------------------------------------------------------
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
#include <linux/atomic.h>
/* #include <asm/atomic.h> */
/* #include <asm/system.h> */
/* #include <linux/xlog.h> */
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "gc8034_mipi_Sensor.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "GC8034_camera_sensor"
#define LOG_1 LOG_INF("GC8034,MIPI 2LANE\n")
/****************************   Modify end    *******************************************/
#define GC8034_DEBUG
#if defined(GC8034_DEBUG)
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
static int otp_read_success;
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static kal_uint32 Dgain_ratio = 256;
static kal_uint8  BorF = 1;
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = GC8034_SENSOR_ID,       /*record sensor id defined in Kd_imgsensor.h*/

	.checksum_value = 0x1b375588,//17201535 cb7 181013  /*checksum value for Camera Auto Test*/
	.pre = {
		.pclk = 320000000,                /*record different mode's pclk*/
		.linelength = 4272,                /*record different mode's linelength*/
		.framelength = 2500,            /*record different mode's framelength*/
		.startx = 0,                    /*record different mode's startx of grabwindow*/
		.starty = 0,                    /*record different mode's starty of grabwindow*/
		.grabwindow_width = 1632,        /*record different mode's width of grabwindow */
		.grabwindow_height = 1224,        /*record different mode's height of grabwindow */
		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
		.mipi_data_lp2hs_settle_dc = 85,/*unit , ns*/
		.mipi_pixel_rate = 134400000,
		/* following for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 320000000,                /*record different mode's pclk*/
		.linelength = 4272,               /*record different mode's linelength*/
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85, /*unit, ns*/
		.mipi_pixel_rate = 257200000,
		.max_framerate = 300,
	},
	.cap1 = {
		/* capture for PIP 24fps relative information */
		/* capture1 mode must use same framelength */
		/* linelength with Capture mode for shutter calculate */
		.pclk = 320000000,                /*record different mode's pclk*/
		.linelength = 4272,               /*record different mode's linelength*/
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,/*unit , ns*/
		.mipi_pixel_rate = 257200000,
		/* less than 13M(include 13M),cap1 max framerate is 24fps */
		/* 16M max framerate is 20fps, 20M max framerate is 15fps */
		.max_framerate = 240,
	},
	.normal_video = {
		.pclk = 320000000,                /*record different mode's pclk*/
		.linelength = 4272,               /*record different mode's linelength*/
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,/*unit , ns*/
		.mipi_pixel_rate = 257200000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 320000000,                /*record different mode's pclk*/
		.linelength = 4272,               /*record different mode's linelength*/
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,/*unit , ns*/
		.mipi_pixel_rate = 134400000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 320000000,                /*record different mode's pclk*/
		.linelength = 4272,               /*record different mode's linelength*/
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,/*unit , ns*/
		.mipi_pixel_rate = 134400000,
		.max_framerate = 300,
	},
	.margin = 16,            /* sensor framelength & shutter margin */
	.min_shutter = 4,                     /* min shutter */
	.max_frame_length = 0x2800,           /* max framelength by sensor register's limitation */
	.ae_shut_delay_frame = 0,
	/* shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2 */
	.ae_sensor_gain_delay_frame = 0,
	/* sensor gain delay frame for AE cycle, */
	/* 2 frame with ispGain_delay-sensor_gain_delay=2-0=2 */
	.ae_ispGain_delay_frame = 2,          /* isp gain delay frame for AE cycle */
	.ihdr_support = 0,                    /* 1, support; 0,not support */
	.ihdr_le_firstline = 0,               /* 1,le first ; 0, se first */
	.sensor_mode_num = 5,                 /* support sensor mode num */
	.cap_delay_frame = 2,                 /* enter capture delay frame num */
	.pre_delay_frame = 2,                 /* enter preview delay frame num */
	.video_delay_frame = 2,               /* enter video delay frame num */
	.hs_video_delay_frame = 2,            /* enter high speed video  delay frame num */
	.slim_video_delay_frame = 2,          /* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_8MA,   /* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,    /* sensor_interface_type */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,                    /* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
#if defined(GC8034_MIRROR_NORMAL)
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R, /*sensor output first pixel color*/
#elif defined(GC8034_MIRROR_H)
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,/*sensor output first pixel color*/
#elif defined(GC8034_MIRROR_V)
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,/*sensor output first pixel color*/
#elif defined(GC8034_MIRROR_HV)
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B, /*sensor output first pixel color*/
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R, /*sensor output first pixel color*/
#endif
	.mclk = 24,                                             /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_2_LANE,                    /* mipi lane num */
	.i2c_addr_table = { 0x6e, 0xff },
	/* record sensor support all write id addr, only supprt 4must end with 0xff */
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,            /* IMAGE_NORMAL, mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode */
	/* such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video */
	.shutter = 0x3ED,                     /* current shutter */
	.gain = 0x40,                         /* current gain */
	.dummy_pixel = 0,                     /* current dummypixel */
	.dummy_line = 0,                      /* current dummyline */
	.current_fps = 300,                   /* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,
	/* auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker */
	.test_pattern = KAL_FALSE,
	/* test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_en = 0,                         /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x6e,                 /* record current sensor's i2c write id */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT
	imgsensor_winsize_info[5] = {
	{ 3264, 2448,   0,   0, 3264, 2448, 1632, 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224}, /* Preview */
	{ 3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, /* capture */
	{ 3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, /* video */
	{ 3264, 2448,   0,   0, 3264, 2448, 1632, 1224,
	0, 0, 1632, 1224, 0, 0, 1632, 1224},
	{ 3264, 2448,   0,   0, 3264, 2448, 1632, 1224,
	0, 0, 1632, 1224, 0, 0, 1632, 1224}
};
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1,
		    imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}


static struct gc8034_otp_t gc8034_otp_info = {0};

static kal_uint8 gc8034_read_otp(kal_uint8 page, kal_uint8 addr)
{
	LOG_INF("E!\n");
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xd4,
			  ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	write_cmos_sensor(0xd5, (addr << 3) & 0xff);
	mdelay(1);
	write_cmos_sensor(0xf3, 0x20);

	return  read_cmos_sensor(0xd7);
}

static void gc8034_read_otp_kgroup(kal_uint8 page,
				   kal_uint8 addr, kal_uint8 *buff, int size)
{
	kal_uint16 i;
	kal_uint8 regf4 = read_cmos_sensor(0xf4);
	write_cmos_sensor(0xd4,
			  ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	write_cmos_sensor(0xd5, (addr << 3) & 0xff);
	mdelay(1);
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf4, regf4 | 0x02);
	write_cmos_sensor(0xf3, 0x80);
	for (i = 0; i < size; i++) {
		if (((addr + i) % 0x80) == 0) {
			write_cmos_sensor(0xf3, 0x00);
			write_cmos_sensor(0xf4, regf4 & 0xfd);
			write_cmos_sensor(0xd4, ((++page << 2) & 0x3c));
			write_cmos_sensor(0xd5, 0x00);
			mdelay(1);
			write_cmos_sensor(0xf3, 0x20);
			write_cmos_sensor(0xf4, regf4 | 0x02);
			write_cmos_sensor(0xf3, 0x80);
		}
		buff[i] = read_cmos_sensor(0xd7);
	}
	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xf4, regf4 & 0xfd);
}

#if defined(GC8034OTP_FOR_CUSTOMER)
static void gc8034_read_otp_group(kal_uint8 page, kal_uint8 addr,
				  kal_uint8 *buff, int size)
{
	kal_uint8 i;
	kal_uint8 regf4 = read_cmos_sensor(0xf4);

	write_cmos_sensor(0xd4,
			  ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	write_cmos_sensor(0xd5, (addr << 3) & 0xff);
	mdelay(1);
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf4, regf4 | 0x02);
	write_cmos_sensor(0xf3, 0x80);

	for (i = 0; i < size; i++)
		buff[i] = read_cmos_sensor(0xd7);

	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xf4, regf4 & 0xfd);
}
#endif

static void gc8034_gcore_read_otp_info(void)
{
	kal_uint8  flagdd = 0;
	kal_uint16 i = 0, j = 0;
	kal_uint8  temp = 0;
	kal_uint8  total_number = 0, cnt = 0;
	kal_uint8  ddtempbuff[4 * 80] = { 0 };
#if defined(GC8034OTP_FOR_CUSTOMER)
	kal_uint32 check = 0;
	kal_uint8  flag_wb = 0, index = 0, flag_module = 0, flag_lsc = 0,
		   flag_af = 0;
	kal_uint8  info[8] = { 0 };
	kal_uint8  wb[4] = { 0 };
	kal_uint8  golden[4] = { 0 };
	kal_uint8  af[4] = { 0 };
	kal_uint8  lsc_checksum = 0;
#endif
#if defined(GC8034OTP_DEBUG)
	kal_uint8  debug[128] = { 0 };
#endif
	//ITD:Modify Camera Perf Opti 20180820 Start
#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	if ((otp_read_success & 0x01) == 0x01) {
		LOG_INF("Read OTP Success Already Return\n");
		return;
	}
#endif
	//ITD:Modify Camera Perf Opti 20180820 End
	memset(&gc8034_otp_info, 0, sizeof(struct gc8034_otp_t));

	/* Static Defective Pixel */
	flagdd = gc8034_read_otp(0, 0x0b);
	LOG_INF("GC8034_OTP_DD : flag_dd = 0x%x\n", flagdd);

	switch (flagdd & 0x03) {
	case 0x00:
		LOG_INF("GC8034_OTP_DD is Empty !!\n");
		gc8034_otp_info.dd_flag = 0x00;
		break;
	case 0x01:
		LOG_INF("GC8034_OTP_DD is Valid!!\n");
		total_number = gc8034_read_otp(0, 0x0c) + gc8034_read_otp(0,
				0x0d);
		gc8034_otp_info.dd_flag = 0x01;
		gc8034_read_otp_kgroup(0, 0x0e, &ddtempbuff[0],
				       4 * total_number);
		for (i = 0; i < total_number; i++) {
			if ((ddtempbuff[4 * i + 3] & 0x80) == 0x80) {
				if ((ddtempbuff[4 * i + 3] & 0x03) == 0x03) {
					gc8034_otp_info.dd_param[cnt].x = (((kal_uint16)ddtempbuff[4 * i
									    + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
					gc8034_otp_info.dd_param[cnt].y = ((kal_uint16)ddtempbuff[4 * i +
									   2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4);
					gc8034_otp_info.dd_param[cnt++].t = 2;
					gc8034_otp_info.dd_param[cnt].x = (((kal_uint16)ddtempbuff[4 * i
									    + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
					gc8034_otp_info.dd_param[cnt].y = ((kal_uint16)ddtempbuff[4 * i +
									   2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4) + 1;
					gc8034_otp_info.dd_param[cnt++].t = 2;
				} else {
					gc8034_otp_info.dd_param[cnt].x = (((kal_uint16)ddtempbuff[4 * i
									    + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
					gc8034_otp_info.dd_param[cnt].y = ((kal_uint16)ddtempbuff[4 * i +
									   2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4);
					gc8034_otp_info.dd_param[cnt++].t = ddtempbuff[4 * i + 3] & 0x03;
				}
			}
		}
		gc8034_otp_info.dd_cnt = cnt;
		LOG_INF("GC8034_OTP : total_number = %d\n",
			gc8034_otp_info.dd_cnt);
		break;
	case 0x02:
	case 0x03:
		LOG_INF("GC8034_OTP_DD is Invalid !!\n");
		gc8034_otp_info.dd_flag = 0x02;
		break;
	default:
		break;
	}

#if defined(GC8034OTP_FOR_CUSTOMER)
	flag_module = gc8034_read_otp(9, 0x6f);
	flag_wb = gc8034_read_otp(9, 0x5e);
	flag_af = gc8034_read_otp(3, 0x3a);
	LOG_INF("GC8034_OTP : flag_module = 0x%x , flag_wb = 0x%x , flag_af = 0x%x\n",
		flag_module, flag_wb, flag_af);

	/* INFO & WB & AF */
	for (index = 0; index < 2; index++) {
		switch ((flag_module << (2 * index)) & 0x0c) {
		case 0x00:
			LOG_INF("GC8034_OTP_INFO group %d is Empty !!\n", index + 1);
			break;
		case 0x04:
			LOG_INF("GC8034_OTP_INFO group %d is Valid !!\n", index + 1);
			check = 0;
			gc8034_read_otp_group(9, INFO_ROM_START + index * INFO_WIDTH,
					      &info[0], INFO_WIDTH);
			for (i = 0; i < INFO_WIDTH - 1; i++)
				check += info[i];

			if ((check % 255 + 1) == info[INFO_WIDTH - 1]) {
				gc8034_otp_info.module_id = info[0];
				gc8034_otp_info.lens_id = info[1];
				gc8034_otp_info.vcm_driver_id = info[2];
				gc8034_otp_info.vcm_id = info[3];
				gc8034_otp_info.year = info[4];
				gc8034_otp_info.month = info[5];
				gc8034_otp_info.day = info[6];
			} else
				LOG_INF("GC8034_OTP_INFO Check sum %d Error !!\n", index + 1);
			break;
		case 0x08:
		case 0x0c:
			LOG_INF("GC8034_OTP_INFO group %d is Invalid !!\n", index + 1);
			break;
		default:
			break;
		}

		switch ((flag_wb << (2 * index)) & 0x0c) {
		case 0x00:
			LOG_INF("GC8034_OTP_WB group %d is Empty !!\n", index + 1);
			gc8034_otp_info.wb_flag = gc8034_otp_info.wb_flag | 0x00;
			break;
		case 0x04:
			LOG_INF("GC8034_OTP_WB group %d is Valid !!\n", index + 1);
			check = 0;
			gc8034_read_otp_group(9, WB_ROM_START + index * WB_WIDTH, &wb[0],
					      WB_WIDTH);
			for (i = 0; i < WB_WIDTH - 1; i++)
				check += wb[i];

			if ((check % 255 + 1) == wb[WB_WIDTH - 1]) {
				gc8034_otp_info.rg_gain = (wb[0] | ((wb[1] & 0xf0) << 4)) > 0 ?
							  (wb[0] | ((wb[1] & 0xf0) << 4)) : 0x400;
				gc8034_otp_info.bg_gain = (((wb[1] & 0x0f) << 8) | wb[2]) > 0 ?
							  (((wb[1] & 0x0f) << 8) | wb[2]) : 0x400;
				gc8034_otp_info.wb_flag = gc8034_otp_info.wb_flag | 0x01;
			} else
				LOG_INF("GC8034_OTP_WB Check sum %d Error !!\n", index + 1);
			break;
		case 0x08:
		case 0x0c:
			LOG_INF("GC8034_OTP_WB group %d is Invalid !!\n", index + 1);
			gc8034_otp_info.wb_flag = gc8034_otp_info.wb_flag | 0x02;
			break;
		default:
			break;
		}

		switch ((flag_wb << (2 * index)) & 0xc0) {
		case 0x00:
			LOG_INF("GC8034_OTP_GOLDEN group %d is Empty !!\n", index + 1);
			gc8034_otp_info.golden_flag = gc8034_otp_info.golden_flag | 0x00;
			break;
		case 0x40:
			LOG_INF("GC8034_OTP_GOLDEN group %d is Valid !!\n", index + 1);
			check = 0;
			gc8034_read_otp_group(9, GOLDEN_ROM_START + index * GOLDEN_WIDTH,
					      &golden[0], GOLDEN_WIDTH);
			for (i = 0; i < GOLDEN_WIDTH - 1; i++)
				check += golden[i];

			if ((check % 255 + 1) == golden[GOLDEN_WIDTH - 1]) {
				gc8034_otp_info.golden_rg = (golden[0] | ((golden[1] & 0xf0) <<
							     4)) > 0 ?
							    (golden[0] | ((golden[1] & 0xf0) << 4)) : RG_TYPICAL;
				gc8034_otp_info.golden_bg = (((golden[1] & 0x0f) << 8) |
							     golden[2]) > 0 ?
							    (((golden[1] & 0x0f) << 8) | golden[2]) : BG_TYPICAL;
				gc8034_otp_info.golden_flag = gc8034_otp_info.golden_flag | 0x01;
			} else
				LOG_INF("GC8034_OTP_GOLDEN Check sum %d Error !!\n", index + 1);

			break;
		case 0x80:
		case 0xc0:
			LOG_INF("GC8034_OTP_GOLDEN group %d is Invalid !!\n", index + 1);
			gc8034_otp_info.golden_flag = gc8034_otp_info.golden_flag | 0x02;
			break;
		default:
			break;
		}

		switch ((flag_af << (2 * index)) & 0x0c) {
		case 0x00:
			LOG_INF("GC8034_OTP_AF group %d is Empty !!\n", index + 1);
			gc8034_otp_info.af_flag = gc8034_otp_info.af_flag | 0x00;
			break;
		case 0x04:
			LOG_INF("GC8034_OTP_AF group %d is Valid !!\n", index + 1);
			check = 0;
			gc8034_read_otp_group(3, AF_ROM_START + index * AF_WIDTH, &af[0],
					      AF_WIDTH);
			for (i = 0; i < AF_WIDTH - 1; i++)
				check += af[i];

			if ((check % 255 + 1) == af[AF_WIDTH - 1]) {
				gc8034_otp_info.af_infinity = ((af[0] << 4) & 0x0f00) + af[1];
				gc8034_otp_info.af_macro = ((af[0] << 8) & 0x0f00) + af[2];
				gc8034_otp_info.af_flag = gc8034_otp_info.af_flag | 0x01;
			} else
				LOG_INF("GC8034_OTP_AF Check sum %d Error !!\n", index + 1);
			break;
		case 0x08:
		case 0x0c:
			LOG_INF("GC8034_OTP_AF group %d is Invalid !!\n", index + 1);
			gc8034_otp_info.af_flag = gc8034_otp_info.af_flag | 0x02;
			break;
		default:
			break;
		}
	}

	/* LSC */
	flag_lsc = gc8034_read_otp(3, 0x43);
	LOG_INF("GC8034_OTP_LSC : flag_lsc = 0x%x\n", flag_lsc);
	gc8034_otp_info.lsc_flag = 0;
	for (index = 0; index < 2; index++) {
		switch ((flag_lsc << (2 * index)) & 0x0c) {
		case 0x00:
			LOG_INF("GC8034_OTP_LSC group %d is Empty !\n", index + 1);
			gc8034_otp_info.lsc_flag = gc8034_otp_info.lsc_flag | 0x00;
			break;
		case 0x04:
			LOG_INF("GC8034_OTP_LSC group %d is Valid !\n", index + 1);
			check = 0;
			if (index == 0) {
				gc8034_read_otp_kgroup(3, 0x44, &gc8034_otp_info.lsc_param[0],
						       396);
				lsc_checksum = gc8034_read_otp(6, 0x50);
			} else {
				gc8034_read_otp_kgroup(6, 0x51, &gc8034_otp_info.lsc_param[0],
						       396);
				lsc_checksum = gc8034_read_otp(9, 0x5d);
			}
			for (i = 0; i < 396; i++)
				check += gc8034_otp_info.lsc_param[i];
			if ((check % 255 + 1) == lsc_checksum)
				gc8034_otp_info.lsc_flag = gc8034_otp_info.lsc_flag | 0x01;
			else {
				gc8034_otp_info.lsc_flag = gc8034_otp_info.lsc_flag | 0x02;
				LOG_INF("GC8034_OTP_LSC check sum error, check sum read : 0x%x, calculate:0x%x!!\n",
					lsc_checksum, check % 255 + 1);
			}
			break;
		case 0x08:
		case 0x0c:
			LOG_INF("GC8034_OTP_LSC group %d is Invalid !!\n", index + 1);
			gc8034_otp_info.lsc_flag = gc8034_otp_info.lsc_flag | 0x02;
			break;
		default:
			break;
		}
	}

	/* print otp information */
	LOG_INF("GC8034_OTP_INFO:module_id=0x%x\n",
		gc8034_otp_info.module_id);
	LOG_INF("GC8034_OTP_INFO:lens_id=0x%x\n",
		gc8034_otp_info.lens_id);
	LOG_INF("GC8034_OTP_INFO:vcm_id=0x%x\n", gc8034_otp_info.vcm_id);
	LOG_INF("GC8034_OTP_INFO:vcm_driver_id=0x%x\n",
		gc8034_otp_info.vcm_driver_id);
	LOG_INF("GC8034_OTP_INFO:data=%d-%d-%d\n", gc8034_otp_info.year,
		gc8034_otp_info.month, gc8034_otp_info.day);
	LOG_INF("GC8034_OTP_WB:r/g=0x%x\n", gc8034_otp_info.rg_gain);
	LOG_INF("GC8034_OTP_WB:b/g=0x%x\n", gc8034_otp_info.bg_gain);
	LOG_INF("GC8034_OTP_GOLDEN:golden_rg=0x%x\n",
		gc8034_otp_info.golden_rg);
	LOG_INF("GC8034_OTP_GOLDEN:golden_bg=0x%x\n",
		gc8034_otp_info.golden_bg);
	LOG_INF("GC8034_OTP_AF:infitiy=0x%x\n",
		gc8034_otp_info.af_infinity);
	LOG_INF("GC8034_OTP_AF:macro=0x%x\n", gc8034_otp_info.af_macro);
#endif

	/* chip regs */
	gc8034_otp_info.reg_flag = gc8034_read_otp(2, 0x4e);

	if (gc8034_otp_info.reg_flag == 1)
		for (i = 0; i < 5; i++) {
			temp = gc8034_read_otp(2, 0x4f + 5 * i);
			for (j = 0; j < 2; j++)
				if (((temp >> (4 * j + 3)) & 0x01) == 0x01) {
					gc8034_otp_info.reg_page[gc8034_otp_info.reg_num] = (temp >>
							(4 * j)) & 0x03;
					gc8034_otp_info.reg_addr[gc8034_otp_info.reg_num] =
						gc8034_read_otp(2, 0x50 + 5 * i + 2 * j);
					gc8034_otp_info.reg_value[gc8034_otp_info.reg_num] =
						gc8034_read_otp(2, 0x50 + 5 * i + 2 * j + 1);
					gc8034_otp_info.reg_num++;
				}
		}

#if defined(GC8034OTP_DEBUG)
	for (i = 0; i < 10; i++) {
		gc8034_read_otp_group(i, 0, &debug[0], 128);
		for (j = 0; j < 128; j++)
			LOG_INF("GC8034_OTP_DEBUG: Page%d: addr = 0x%x, value = 0x%x;\n",
				i, j, debug[j]);
	}
#endif

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	otp_read_success = gc8034_otp_info.wb_flag &
			   gc8034_otp_info.lsc_flag & gc8034_otp_info.golden_flag;
#endif

}

static void gc8034_gcore_check_prsel(void)
{
	kal_uint8 product_level = 0;

	product_level = gc8034_read_otp(2, 0x68) & 0x07;

	if ((product_level == 0x00) || (product_level == 0x01)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xd2, 0xcb);
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xd2, 0xc3);
	}
}

static void gc8034_gcore_update_dd(void)
{
	kal_uint8 i = 0, j = 0;
	kal_uint8 temp_val0 = 0, temp_val1 = 0, temp_val2 = 0;
	struct gc8034_dd_t dd_temp = {0, 0, 0};
	if (gc8034_otp_info.dd_flag == 0x01) {
		LOG_INF("GC8034_OTP_AUTO_DD start !\n");
		for (i = 0; i < gc8034_otp_info.dd_cnt; i++) {
#if defined(GC8034_MIRROR_H) || defined(GC8034_MIRROR_HV)
			switch (gc8034_otp_info.dd_param[i].t) {
			case 0:
				gc8034_otp_info.dd_param[i].x = DD_WIDTH -
								gc8034_otp_info.dd_param[i].x + 1;
				break;
			case 1:
				gc8034_otp_info.dd_param[i].x = DD_WIDTH -
								gc8034_otp_info.dd_param[i].x - 1;
				break;
			default:
				gc8034_otp_info.dd_param[i].x = DD_WIDTH -
								gc8034_otp_info.dd_param[i].x;
				break;
			}
#endif
#if defined(GC8034_MIRROR_V) || defined(GC8034_MIRROR_HV)
			gc8034_otp_info.dd_param[i].y = DD_HEIGHT - gc8034_otp_info.
							dd_param[i].y + 1;
#endif
		}
		for (i = 0; i < gc8034_otp_info.dd_cnt - 1; i++) {
			for (j = i + 1; j < gc8034_otp_info.dd_cnt; j++) {
				if (gc8034_otp_info.dd_param[i].y * DD_WIDTH +
				    gc8034_otp_info.dd_param[i].x
				    > gc8034_otp_info.dd_param[j].y * DD_WIDTH +
				    gc8034_otp_info.dd_param[j].x) {
					dd_temp.x = gc8034_otp_info.dd_param[i].x;
					dd_temp.y = gc8034_otp_info.dd_param[i].y;
					dd_temp.t = gc8034_otp_info.dd_param[i].t;
					gc8034_otp_info.dd_param[i].x = gc8034_otp_info.dd_param[j].x;
					gc8034_otp_info.dd_param[i].y = gc8034_otp_info.dd_param[j].y;
					gc8034_otp_info.dd_param[i].t = gc8034_otp_info.dd_param[j].t;
					gc8034_otp_info.dd_param[j].x = dd_temp.x;
					gc8034_otp_info.dd_param[j].y = dd_temp.y;
					gc8034_otp_info.dd_param[j].t = dd_temp.t;
				}
			}
		}
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0xbe, 0x00);
		write_cmos_sensor(0xa9, 0x01);
		for (i = 0; i < gc8034_otp_info.dd_cnt; i++) {
			temp_val0 = gc8034_otp_info.dd_param[i].x & 0x00ff;
			temp_val1 = ((gc8034_otp_info.dd_param[i].y & 0x000f) << 4) + ((
						gc8034_otp_info.dd_param[i].x & 0x0f00) >> 8);
			temp_val2 = (gc8034_otp_info.dd_param[i].y & 0x0ff0) >> 4;
			write_cmos_sensor(0xaa, i);
			write_cmos_sensor(0xac, temp_val0);
			write_cmos_sensor(0xac, temp_val1);
			write_cmos_sensor(0xac, temp_val2);
			write_cmos_sensor(0xac, gc8034_otp_info.dd_param[i].t);

			LOG_INF("GC8034_OTP_GC val0 = 0x%x , val1 = 0x%x , val2 = 0x%x \n",
				temp_val0, temp_val1, temp_val2);
			LOG_INF("GC8034_OTP_GC x = %d , y = %d \n",
				((temp_val1 & 0x0f) << 8) + temp_val0,
				(temp_val2 << 4) + ((temp_val1 & 0xf0) >> 4));
		}

		write_cmos_sensor(0xbe, 0x01);
		write_cmos_sensor(0xfe, 0x00);
	}
}

#if defined(GC8034OTP_FOR_CUSTOMER)
static void gc8034_gcore_update_wb(void)
{
	kal_uint16 r_gain_current = 0, g_gain_current = 0,
		   b_gain_current = 0, base_gain = 0;
	kal_uint16 r_gain = 1024, g_gain = 1024, b_gain = 1024;
	kal_uint16 rg_typical = 0, bg_typical = 0;

	if (0x01 == (gc8034_otp_info.golden_flag & 0x01)) {
		rg_typical = gc8034_otp_info.golden_rg;
		bg_typical = gc8034_otp_info.golden_bg;
		LOG_INF("GC8034_OTP_UPDATE_AWB:rg_typical = 0x%x, bg_typical = 0x%x\n",
			rg_typical, bg_typical);
	} else {
		rg_typical = RG_TYPICAL;
		bg_typical = BG_TYPICAL;
		LOG_INF("GC8034_OTP_UPDATE_AWB:rg_typical = 0x%x, bg_typical = 0x%x\n",
			rg_typical, bg_typical);
	}

	if (0x01 == (gc8034_otp_info.wb_flag & 0x01)) {
		r_gain_current = 2048 * rg_typical / gc8034_otp_info.rg_gain;
		b_gain_current = 2048 * bg_typical / gc8034_otp_info.bg_gain;
		g_gain_current = 2048;

		base_gain = (r_gain_current < b_gain_current) ? r_gain_current :
			    b_gain_current;
		base_gain = (base_gain < g_gain_current) ? base_gain :
			    g_gain_current;

		r_gain = 0x400 * r_gain_current / base_gain;
		g_gain = 0x400 * g_gain_current / base_gain;
		b_gain = 0x400 * b_gain_current / base_gain;
		LOG_INF("GC8034_OTP_UPDATE_AWB:r = 0x%x, g = 0x%x, b = 0x%x\n",
			r_gain, g_gain, b_gain);

		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0x84, g_gain >> 3);
		write_cmos_sensor(0x85, r_gain >> 3);
		write_cmos_sensor(0x86, b_gain >> 3);
		write_cmos_sensor(0x87, g_gain >> 3);
		write_cmos_sensor(0x88,
				  ((g_gain & 0x07) << 4) + (r_gain & 0x07));
		write_cmos_sensor(0x89,
				  ((b_gain & 0x07) << 4) + (g_gain & 0x07));
		write_cmos_sensor(0xfe, 0x00);
	}
}

static void gc8034_gcore_update_lsc(void)
{
	kal_uint8 i = 0, j = 0;
	kal_uint16 base = 0;

	if ((gc8034_otp_info.lsc_flag & 0x01) == 0x01) {
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0xcf, 0x00);
		write_cmos_sensor(0xc9, 0x01);
		for (i = 0; i < 9; i++) {
			write_cmos_sensor(0xca, i * 0x0c);
			for (j = 0; j < 11; j++) {
#if defined(GC8034_MIRROR_NORMAL)
				base = 4 * (11 * i + j);
#elif defined(GC8034_MIRROR_H)
				base = 4 * (11 * i + 10 - j);
#elif defined(GC8034_MIRROR_V)
				base = 4 * (11 * (8 - i) + j);
#elif defined(GC8034_MIRROR_HV)
				base = 4 * (11 * (8 - i) + 10 - j);
#endif
				write_cmos_sensor(0xcc, gc8034_otp_info.lsc_param[base + 0]);
				write_cmos_sensor(0xcc, gc8034_otp_info.lsc_param[base + 1]);
				write_cmos_sensor(0xcc, gc8034_otp_info.lsc_param[base + 2]);
				write_cmos_sensor(0xcc, gc8034_otp_info.lsc_param[base + 3]);
			}
		}
		write_cmos_sensor(0xcf, 0x01);
		write_cmos_sensor(0xa0, 0x13);
		write_cmos_sensor(0xfe, 0x00);
	}
}
#endif
static void gc8034_gcore_update_chipversion(void)
{
	kal_uint8 i = 0;

	LOG_INF("GC8034_OTP_UPDATE_CHIPVERSION:reg_num = %d\n",
		gc8034_otp_info.reg_num);

	if (gc8034_otp_info.reg_flag)
		for (i = 0; i < gc8034_otp_info.reg_num; i++) {
			write_cmos_sensor(0xfe, gc8034_otp_info.reg_page[i]);
			write_cmos_sensor(gc8034_otp_info.reg_addr[i],
					  gc8034_otp_info.reg_value[i]);
			LOG_INF("GC8034_OTP_UPDATE_CHIP_VERSION: P%d:0x%x -> 0x%x\n",
				gc8034_otp_info.reg_page[i], gc8034_otp_info.reg_addr[i],
				gc8034_otp_info.reg_value[i]);
		}
}

static void gc8034_gcore_update_otp(void)
{
	gc8034_gcore_update_dd();
	gc8034_gcore_check_prsel();
#if defined(GC8034OTP_FOR_CUSTOMER)
	gc8034_gcore_update_wb();
	gc8034_gcore_update_lsc();
#endif
	gc8034_gcore_update_chipversion();
}

static void gc8034_gcore_enable_otp(kal_uint8 state)
{
	kal_uint8 otp_clk = 0, otp_en = 0;

	otp_clk = read_cmos_sensor(0xf2);
	otp_en = read_cmos_sensor(0xf4);
	if (state) {
		otp_clk = otp_clk | 0x01;
		otp_en = otp_en | 0x08;
		write_cmos_sensor(0xf2, otp_clk);
		write_cmos_sensor(0xf4, otp_en);
		LOG_INF("GC8034_OTP: Enable OTP!\n");
	} else {
		otp_en = otp_en & 0xf7;
		otp_clk = otp_clk & 0xfe;
		write_cmos_sensor(0xf4, otp_en);
		write_cmos_sensor(0xf2, otp_clk);
		LOG_INF("GC8034_OTP: Disable OTP!\n");
	}
}

static void gc8034_gcore_identify_otp(void)
{
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xf7, 0x97);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0xee);
	gc8034_gcore_enable_otp(otp_open);
	gc8034_gcore_read_otp_info();
	gc8034_gcore_update_otp();
	gc8034_gcore_enable_otp(otp_close);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xf7, 0x95);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0xee);
}

static void set_dummy(void)
{
	kal_uint32  vb = 16;
	kal_uint32  basic_line = 2484;

	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	vb = imgsensor.frame_length - basic_line;
	vb = vb < 16 ? 16 : vb;
	vb = vb > 8191 ? 8191 : vb;
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x07, (vb >> 8) & 0x1f);
	write_cmos_sensor(0x08, vb & 0xfe);
}

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 sensor_id = 0;

	sensor_id = (read_cmos_sensor(0xf0) << 8) | read_cmos_sensor(
			    0xf1);
	return sensor_id;
}

static void set_max_framerate(kal_uint16 framerate,
			      kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 /
		       imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length >
				  imgsensor.min_frame_length) ?
				 frame_length : imgsensor.min_frame_length;
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
	set_dummy();
}


static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0, cal_shutter = 0;
	/*kal_uint32 frame_length = 0;*/
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_INF("Enter set_shutter!\n");

	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?
		  imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
			      imgsensor_info.margin)) ?
		  (imgsensor_info.max_frame_length - imgsensor_info.margin) :
		  shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
		       imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else {
		set_max_framerate(realtime_fps, 0);
	}

	cal_shutter = shutter >> 1;
	cal_shutter = cal_shutter << 1;
	Dgain_ratio = 256 * shutter / cal_shutter;

	/* Update Shutter */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (cal_shutter >> 8) & 0x7f);
	write_cmos_sensor(0x04, cal_shutter & 0xff);

	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,
		imgsensor.frame_length);
}


static void gain2reg(kal_uint16 gain)
{
	kal_int16 gain_index = 0, temp_gain = 0;
	/*kal_uint8 i = 0;*/
	kal_uint16 gain_level[MAX_AG_INDEX] = {
		0x0040, /* 1.000*/
		0x0058, /* 1.375*/
		0x007d, /* 1.950*/
		0x00ad, /* 2.700*/
		0x00f3, /* 3.800*/
		0x0159, /* 5.400*/
		0x01ea, /* 7.660*/
		0x02ac, /*10.688*/
		0x03c2, /*15.030*/
	};
	kal_uint8 agc_register[2 * MAX_AG_INDEX][AGC_REG_NUM] = {
		/*{ 0xfe, 0x20, 0x33, 0xd0, 0xfe, 0xdf, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xfe },*/
		/* binning */
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50, 0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50, 0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x4e, 0x84, 0x01, 0x0c, 0x2e, 0x2d, 0x15, 0x19, 0x47, 0x70, 0x9f, 0xd8, 0x00 },
		{ 0x00, 0x51, 0x80, 0x01, 0x07, 0x28, 0x32, 0x22, 0x20, 0x49, 0x70, 0x91, 0xd9, 0x00 },
		{ 0x00, 0x4d, 0x83, 0x01, 0x0f, 0x3b, 0x3b, 0x1c, 0x1f, 0x47, 0x6f, 0x9b, 0xd3, 0x00 },
		{ 0x00, 0x50, 0x83, 0x01, 0x08, 0x35, 0x46, 0x1e, 0x22, 0x4c, 0x70, 0x9a, 0xd2, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		/* fullsize */
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50, 0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50, 0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x4e, 0x84, 0x01, 0x0c, 0x2e, 0x2d, 0x15, 0x19, 0x47, 0x70, 0x9f, 0xd8, 0x00 },
		{ 0x00, 0x51, 0x80, 0x01, 0x07, 0x28, 0x32, 0x22, 0x20, 0x49, 0x70, 0x91, 0xd9, 0x00 },
		{ 0x00, 0x4d, 0x83, 0x01, 0x0f, 0x3b, 0x3b, 0x1c, 0x1f, 0x47, 0x6f, 0x9b, 0xd3, 0x00 },
		{ 0x00, 0x50, 0x83, 0x01, 0x08, 0x35, 0x46, 0x1e, 0x22, 0x4c, 0x70, 0x9a, 0xd2, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 }
	};

	for (gain_index = MEAG_INDEX - 1; gain_index >= 0; gain_index--)
		if (gain >= gain_level[gain_index]) {
			write_cmos_sensor(0xb6, gain_index);
			temp_gain = 256 * gain / gain_level[gain_index];
			temp_gain = temp_gain * Dgain_ratio / 256;
			write_cmos_sensor(0xb1, temp_gain >> 8);
			write_cmos_sensor(0xb2, temp_gain & 0xff);

			write_cmos_sensor(0xfe,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][0]);
			write_cmos_sensor(0x20,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][1]);
			write_cmos_sensor(0x33,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][2]);
			write_cmos_sensor(0xfe,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][3]);
			write_cmos_sensor(0xdf,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][4]);
			write_cmos_sensor(0xe7,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][5]);
			write_cmos_sensor(0xe8,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][6]);
			write_cmos_sensor(0xe9,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][7]);
			write_cmos_sensor(0xea,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][8]);
			write_cmos_sensor(0xeb,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][9]);
			write_cmos_sensor(0xec,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][10]);
			write_cmos_sensor(0xed,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][11]);
			write_cmos_sensor(0xee,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][12]);
			write_cmos_sensor(0xfe,
					  agc_register[BorF * MAX_AG_INDEX + gain_index][13]);
			break;
		}
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 real_gain = 0;

	LOG_INF("Enter set_gain!\n");
	real_gain = (gain < SENSOR_BASE_GAIN) ? SENSOR_BASE_GAIN : gain;
	real_gain = (gain > SENSOR_MAX_GAIN) ? SENSOR_MAX_GAIN : gain;

	gain2reg(real_gain);

	return gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se,
				    kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
}


static void night_mode(kal_bool enable)
{
	LOG_INF("Enter nigh_mode!\n");
	/*No Need to implement this function*/
}

static void sensor_init(void)
{
	LOG_INF("E");
	/* SYS */
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf4, 0x80);
	write_cmos_sensor(0xf5, 0x19);
	write_cmos_sensor(0xf6, 0x44);
	write_cmos_sensor(0xf8, 0x63);
	write_cmos_sensor(0xfa, 0x45);
	write_cmos_sensor(0xf9, 0x00);
	write_cmos_sensor(0xf7, 0x95);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0xea);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x03, 0x9a);
	write_cmos_sensor(0xfc, 0xee);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x88, 0x03);

	/* Cisctl&Analog */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, 0x08);
	write_cmos_sensor(0x04, 0xc6);
	write_cmos_sensor(0x05, 0x02);
	write_cmos_sensor(0x06, 0x16);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x08, 0x10);
	write_cmos_sensor(0x0a, 0x3a);
	write_cmos_sensor(0x0b, 0x00);
	write_cmos_sensor(0x0c, 0x04);
	write_cmos_sensor(0x0d, 0x09);
	write_cmos_sensor(0x0e, 0xa0);
	write_cmos_sensor(0x0f, 0x0c);
	write_cmos_sensor(0x10, 0xd4);
	write_cmos_sensor(0x17, GC8034_MIRROR);
	write_cmos_sensor(0x18, 0x02);
	write_cmos_sensor(0x19, 0x17);
	write_cmos_sensor(0x1e, 0x50);
	write_cmos_sensor(0x1f, 0x80);
	write_cmos_sensor(0x21, 0x4c);
	write_cmos_sensor(0x25, 0x00);
	write_cmos_sensor(0x28, 0x4a);
	write_cmos_sensor(0x2d, 0x89);
	write_cmos_sensor(0xca, 0x02);
	write_cmos_sensor(0xcb, 0x00);
	write_cmos_sensor(0xcc, 0x39);
	write_cmos_sensor(0xce, 0xd0);
	write_cmos_sensor(0xcf, 0x93);
	write_cmos_sensor(0xd0, 0x1b);
	write_cmos_sensor(0xd1, 0xaa);
	write_cmos_sensor(0xd2, 0xcb);
	write_cmos_sensor(0xd8, 0x40);
	write_cmos_sensor(0xd9, 0xff);
	write_cmos_sensor(0xda, 0x0e);
	write_cmos_sensor(0xdb, 0xb0);
	write_cmos_sensor(0xdc, 0x0e);
	write_cmos_sensor(0xde, 0x08);
	write_cmos_sensor(0xe4, 0xc6);
	write_cmos_sensor(0xe5, 0x08);
	write_cmos_sensor(0xe6, 0x10);
	write_cmos_sensor(0xed, 0x2a);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x59, 0x02);
	write_cmos_sensor(0x5a, 0x04);
	write_cmos_sensor(0x5b, 0x08);
	write_cmos_sensor(0x5c, 0x20);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x1a, 0x09);
	write_cmos_sensor(0x1d, 0x13);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);

	/* Gamma */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x20, 0x55);
	write_cmos_sensor(0x33, 0x83);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0xdf, 0x06);
	write_cmos_sensor(0xe7, 0x18);
	write_cmos_sensor(0xe8, 0x20);
	write_cmos_sensor(0xe9, 0x16);
	write_cmos_sensor(0xea, 0x17);
	write_cmos_sensor(0xeb, 0x50);
	write_cmos_sensor(0xec, 0x6c);
	write_cmos_sensor(0xed, 0x9b);
	write_cmos_sensor(0xee, 0xd8);

	/* ISP */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x80, 0x10);
	write_cmos_sensor(0x84, 0x01);
	write_cmos_sensor(0x89, 0x03);
	write_cmos_sensor(0x8d, 0x03);
	write_cmos_sensor(0x8f, 0x14);
	write_cmos_sensor(0xad, 0x30);
	write_cmos_sensor(0x66, 0x2c);
	write_cmos_sensor(0xbc, 0x49);
	write_cmos_sensor(0xc2, 0x7f);
	write_cmos_sensor(0xc3, 0xff);

	/* Crop window */
	write_cmos_sensor(0x90, 0x01);
	write_cmos_sensor(0x92, GC8034_BinStartY);
	write_cmos_sensor(0x94, GC8034_BinStartX);
	write_cmos_sensor(0x95, 0x04);
	write_cmos_sensor(0x96, 0xc8);
	write_cmos_sensor(0x97, 0x06);
	write_cmos_sensor(0x98, 0x60);

	/* Gain */
	write_cmos_sensor(0xb0, 0x90);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xb2, 0x00);
	write_cmos_sensor(0xb6, 0x00);

	/* BLK */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x40, 0x22);
	write_cmos_sensor(0x41, 0x20);
	write_cmos_sensor(0x42, 0x02);
	write_cmos_sensor(0x43, 0x08);
	write_cmos_sensor(0x4e, 0x0f);
	write_cmos_sensor(0x4f, 0xf0);
	write_cmos_sensor(0x58, 0x80);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5a, 0x80);
	write_cmos_sensor(0x5b, 0x80);
	write_cmos_sensor(0x5c, 0x00);
	write_cmos_sensor(0x5d, 0x00);
	write_cmos_sensor(0x5e, 0x00);
	write_cmos_sensor(0x5f, 0x00);
	write_cmos_sensor(0x6b, 0x01);
	write_cmos_sensor(0x6c, 0x00);
	write_cmos_sensor(0x6d, 0x0c);

	/* WB offset */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0xbf, 0x40);

	/* Dark Sun */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x68, 0x77);

	/* DPC */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x60, 0x00);
	write_cmos_sensor(0x61, 0x10);
	write_cmos_sensor(0x62, 0x60);
	write_cmos_sensor(0x63, 0x30);
	write_cmos_sensor(0x64, 0x00);

	/* LSC */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0xa8, 0x60);
	write_cmos_sensor(0xa2, 0xd1);
	write_cmos_sensor(0xc8, 0x57);
	write_cmos_sensor(0xa1, 0xb8);
	write_cmos_sensor(0xa3, 0x91);
	write_cmos_sensor(0xc0, 0x50);
	write_cmos_sensor(0xd0, 0x05);
	write_cmos_sensor(0xd1, 0xb2);
	write_cmos_sensor(0xd2, 0x1f);
	write_cmos_sensor(0xd3, 0x00);
	write_cmos_sensor(0xd4, 0x00);
	write_cmos_sensor(0xd5, 0x00);
	write_cmos_sensor(0xd6, 0x00);
	write_cmos_sensor(0xd7, 0x00);
	write_cmos_sensor(0xd8, 0x00);
	write_cmos_sensor(0xd9, 0x00);
	write_cmos_sensor(0xa4, 0x10);
	write_cmos_sensor(0xa5, 0x20);
	write_cmos_sensor(0xa6, 0x60);
	write_cmos_sensor(0xa7, 0x80);
	write_cmos_sensor(0xab, 0x18);
	write_cmos_sensor(0xc7, 0xc0);

	/* ABB */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x20, 0x02);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x23, 0x42);

	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x07);
	write_cmos_sensor(0x02, 0x03);
	write_cmos_sensor(0x04, 0x80);
	write_cmos_sensor(0x11, 0x2b);
	write_cmos_sensor(0x12, 0xf8);
	write_cmos_sensor(0x13, 0x07);
	write_cmos_sensor(0x15, 0x10);
	write_cmos_sensor(0x16, 0x29);
	write_cmos_sensor(0x17, 0xff);
	write_cmos_sensor(0x18, 0x01);
	write_cmos_sensor(0x19, 0xaa);
	write_cmos_sensor(0x1a, 0x02);
	write_cmos_sensor(0x21, 0x05);
	write_cmos_sensor(0x22, 0x06);
	write_cmos_sensor(0x23, 0x16);
	write_cmos_sensor(0x24, 0x00);
	write_cmos_sensor(0x25, 0x12);
	write_cmos_sensor(0x26, 0x07);
	write_cmos_sensor(0x29, 0x07);
	write_cmos_sensor(0x2a, 0x08);
	write_cmos_sensor(0x2b, 0x07);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x3f, 0x00);
}

static void binning_setting(void)
{
	/* SYS */
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf4, 0x80);
	write_cmos_sensor(0xf5, 0x19);
	write_cmos_sensor(0xf6, 0x44);
	write_cmos_sensor(0xf8, 0x63);
	write_cmos_sensor(0xfa, 0x45);
	write_cmos_sensor(0xf9, 0x00);
	write_cmos_sensor(0xf7, 0x95);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0xea);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x03, 0x9a);
	write_cmos_sensor(0xfc, 0xee);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x3f, 0x00);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);

	/* ISP */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x80, 0x10);
	write_cmos_sensor(0xad, 0x30);
	write_cmos_sensor(0x66, 0x2c);
	write_cmos_sensor(0xbc, 0x49);

	/* Crop window */
	write_cmos_sensor(0x90, 0x01);
	write_cmos_sensor(0x92, GC8034_BinStartY);
	write_cmos_sensor(0x94, GC8034_BinStartX);
	write_cmos_sensor(0x95, 0x04);
	write_cmos_sensor(0x96, 0xc8);
	write_cmos_sensor(0x97, 0x06);
	write_cmos_sensor(0x98, 0x60);



	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x07);
	write_cmos_sensor(0x02, 0x03);
	write_cmos_sensor(0x04, 0x80);
	write_cmos_sensor(0x11, 0x2b);
	write_cmos_sensor(0x12, 0xf8);
	write_cmos_sensor(0x13, 0x07);
	write_cmos_sensor(0x15, 0x10);
	write_cmos_sensor(0x16, 0x29);
	write_cmos_sensor(0x17, 0xff);
	write_cmos_sensor(0x18, 0x01);
	write_cmos_sensor(0x19, 0xaa);
	write_cmos_sensor(0x1a, 0x02);
	write_cmos_sensor(0x21, 0x05);
	write_cmos_sensor(0x22, 0x06);
	write_cmos_sensor(0x23, 0x16);
	write_cmos_sensor(0x24, 0x00);
	write_cmos_sensor(0x25, 0x12);
	write_cmos_sensor(0x26, 0x07);
	write_cmos_sensor(0x29, 0x07);
	write_cmos_sensor(0x2a, 0x08);
	write_cmos_sensor(0x2b, 0x07);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x3f, 0x00);
	BorF = 0;
}

static void fullsize_setting(void)
{
	/* SYS */
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf4, 0x90);
	write_cmos_sensor(0xf5, 0x3d);
	write_cmos_sensor(0xf6, 0x44);
	write_cmos_sensor(0xf8, 0x63);
	write_cmos_sensor(0xfa, 0x42);
	write_cmos_sensor(0xf9, 0x00);
	write_cmos_sensor(0xf7, 0x95);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0x00);
	write_cmos_sensor(0xfc, 0xea);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x03, 0x9a);
	write_cmos_sensor(0xfc, 0xee);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x3f, 0x00);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);

	/* ISP */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x80, 0x13);
	write_cmos_sensor(0xad, 0x00);

	/* Crop window */
	write_cmos_sensor(0x90, 0x01);
	write_cmos_sensor(0x92, GC8034_FullStartY);
	write_cmos_sensor(0x94, GC8034_FullStartX);
	write_cmos_sensor(0x95, 0x09);
	write_cmos_sensor(0x96, 0x90);
	write_cmos_sensor(0x97, 0x0c);
	write_cmos_sensor(0x98, 0xc0);



	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x07);
	write_cmos_sensor(0x02, 0x04);
	write_cmos_sensor(0x04, 0x80);
	write_cmos_sensor(0x11, 0x2b);
	write_cmos_sensor(0x12, 0xf0);
	write_cmos_sensor(0x13, 0x0f);
	write_cmos_sensor(0x15, 0x10);
	write_cmos_sensor(0x16, 0x29);
	write_cmos_sensor(0x17, 0xff);
	write_cmos_sensor(0x18, 0x01);
	write_cmos_sensor(0x19, 0xaa);
	write_cmos_sensor(0x1a, 0x02);
	write_cmos_sensor(0x21, 0x0c);
	write_cmos_sensor(0x22, 0x0c);
	write_cmos_sensor(0x23, 0x56);
	write_cmos_sensor(0x24, 0x00);
	write_cmos_sensor(0x25, 0x1c);
	write_cmos_sensor(0x26, 0x0b);
	write_cmos_sensor(0x29, 0x0e);
	write_cmos_sensor(0x2a, 0x24);
	write_cmos_sensor(0x2b, 0x0b);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x3f, 0x00);
	BorF = 1;
}

static void preview_setting(void)
{
	LOG_INF("Enter preview_setting!\n");
	binning_setting();
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("Enter capture_setting!\n");
	fullsize_setting();
}

static void normal_video_setting(void)
{
	LOG_INF("Enter normal_video_setting!\n");
	fullsize_setting();
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	binning_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	binning_setting();
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_info("streaming_enable(0=Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x3f, 0x91);
		mdelay(10);
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x3f, 0x00);
		mdelay(30);
	}
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0xfc, 0x00);
		write_cmos_sensor(0xf4, 0x80);
		write_cmos_sensor(0xf5, 0x19);
		write_cmos_sensor(0xf8, 0x63);
		write_cmos_sensor(0xfa, 0x45);
		write_cmos_sensor(0xfc, 0x00);
		write_cmos_sensor(0xfc, 0xfe);
		write_cmos_sensor(0xfe, 0x03);
		write_cmos_sensor(0x21, 0x05);
		write_cmos_sensor(0x22, 0x06);
		write_cmos_sensor(0x23, 0x16);
		write_cmos_sensor(0x25, 0x12);
		write_cmos_sensor(0x26, 0x07);
		write_cmos_sensor(0x29, 0x07);
		write_cmos_sensor(0x2a, 0x08);
		write_cmos_sensor(0x2b, 0x07);
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x8c, 0x01);
	} else {
		write_cmos_sensor(0xfc, 0x00);
		write_cmos_sensor(0xf4, 0x90);
		write_cmos_sensor(0xf5, 0x3d);
		write_cmos_sensor(0xf8, 0x63);
		write_cmos_sensor(0xfa, 0x42);
		write_cmos_sensor(0xfc, 0x00);
		write_cmos_sensor(0xfc, 0xee);
		write_cmos_sensor(0xfe, 0x03);
		write_cmos_sensor(0x21, 0x0c);
		write_cmos_sensor(0x22, 0x0e);
		write_cmos_sensor(0x23, 0x45);
		write_cmos_sensor(0x25, 0x1c);
		write_cmos_sensor(0x26, 0x0b);
		write_cmos_sensor(0x29, 0x0e);
		write_cmos_sensor(0x2a, 0x1d);
		write_cmos_sensor(0x2b, 0x0b);
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x8c, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

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
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;

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
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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

	gc8034_gcore_identify_otp();

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
}


static kal_uint32 close(void)
{
	LOG_INF("E\n");
	streaming_control(0);
	return ERROR_NONE;
}


static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}


static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_INF("Enter capture!\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_TRUE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_TRUE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			       *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_INF("Enter normal_video_setting!\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.normal_video.framelength;
	/*imgsensor.current_fps = 300*/
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			   *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_INF("Enter hs_video!\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			     *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_INF("Enter slim_video!\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
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
	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM
			   scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("Enter get_info!\n");
	LOG_INF("scenario_id = %d\n", scenario_id);

	/*sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10;*/
	/* not use */
	/*sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10;*/
	/* not use */
	/*imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate;*/
	/* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5;	 /* not use */

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

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
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
	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
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
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("Enter control!\n");
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
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	/*This Function not used after ROME*/
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	/***********
	 *if (framerate == 0)	 //Dynamic frame rate
	 *	return ERROR_NONE;
	 *spin_lock(&imgsensor_drv_lock);
	 *if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
	 *	imgsensor.current_fps = 296;
	 *else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
	 *	imgsensor.current_fps = 146;
	 *else
	 *	imgsensor.current_fps = framerate;
	 *spin_unlock(&imgsensor_drv_lock);
	 *set_max_framerate(imgsensor.current_fps, 1);
	 ********/
	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable,
					UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else		/* Cancel Auto flick */
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
					imgsensor_info.pre.framelength) ?
				       (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate *
			       10 /
			       imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.normal_video.framelength) ?
				       (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength
					 + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
				       imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
						imgsensor_info.cap1.framelength) ?
					       (frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength +
						 imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate, imgsensor_info.cap.max_framerate / 10);
			frame_length = imgsensor_info.cap.pclk / framerate * 10 /
				       imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
						imgsensor_info.cap.framelength) ?
					       (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength +
						 imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 /
			       imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.hs_video.framelength) ?
				       (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 /
			       imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.slim_video.framelength) ?
				       (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength +
					 imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			       imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
					imgsensor_info.pre.framelength) ?
				       (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
					 imgsensor.dummy_line;
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
	default:
		break;
	}

	return ERROR_NONE;
}



static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM
				  feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *)
					   feature_para;
	/* unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
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
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)*feature_data);
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
		sensor_reg_data->RegData = read_cmos_sensor(
						   sensor_reg_data->RegAddr);
		LOG_INF("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
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
				      *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
					      *feature_data, *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
						  *(feature_data),
						  (MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_info("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_info("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
				*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /*for factory mode auto testing */
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data,
					(UINT16) *(feature_data + 1), (UINT16) *(feature_data + 2));
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
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 GC8034_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT
				  **pfFunc)
{
	/* Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
