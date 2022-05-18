// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx766dualmipiraw_Sensor.c
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
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx766dualmipiraw_Sensor.h"
#include "imx766dual_eeprom.h"

#undef VENDOR_EDIT

#define USE_BURST_MODE 1

/***************Modify Following Strings for Debug**********************/
#define PFX "IMX766DUAL_camera_sensor"
/****************************   Modify end	**************************/
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

static kal_uint8 qsc_flag;
static kal_uint8 otp_flag;

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX766_SENSOR_ID,

	.checksum_value = 0x8ac2d94a,

	.pre = {
		.pclk = 1281600000,
		.linelength = 8816,
		.framelength = 4844,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 510170000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 1281600000,
		.linelength = 8816,
		.framelength = 4844,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 510170000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 1281600000,
		.linelength = 8816,
		.framelength = 4844,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 510170000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 1281600000,
		.linelength = 8816,
		.framelength = 4844,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 510170000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 1281600000,
		.linelength = 8816,
		.framelength = 4844,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 510170000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 1281600000,
		.linelength = 8816,
		.framelength = 4844,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 510170000,
		.max_framerate = 300,
	},
	.custom2 = {
		.pclk = 1281600000,
		.linelength = 8816,
		.framelength = 4840,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 510170000,
		.max_framerate = 300,
	},
	.custom3 = {
		.pclk = 1281600000,
		.linelength = 8816,
		.framelength = 4836,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 510170000,
		.max_framerate = 300,
	},
	.min_gain = 64, /*1x gain*/
	.max_gain = 4096, /*16x gain*/
	.min_gain_iso = 100,
	.margin = 48,		/* sensor framelength & shutter margin */
	.min_shutter = 30,	/* min shutter */
	.gain_step = 1,
	.gain_type = 0,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 0, /* 1, support; 0,not support */
	.sensor_mode_num = 8,	/* support sensor mode num */
	.frame_time_delay_frame = 3,

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,	/* enter custom1 delay frame num */
	.custom2_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom3_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/* .mipi_sensor_type = MIPI_OPHY_NCSI2, */
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_CPHY, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	/*.mipi_lane_num = SENSOR_MIPI_4_LANE,*/
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.i2c_addr_table = {0x34, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_speed = 1000, /* i2c read/write speed */
};
static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34, /* record current sensor's i2c write id */
	.current_ae_effective_frame = 2,
	.extend_frame_length_en = KAL_FALSE,
	.fast_mode_on = KAL_FALSE,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] = {
	{8192, 6144, 000, 000, 8192, 6144, 2048, 1536,
	0000, 0000, 2048, 1536, 0, 0, 2048, 1536}, /* Preview */
	{8192, 6144, 000, 000, 8192, 6144, 2048, 1536,
	0000, 0000, 2048, 1536, 0, 0, 2048, 1536}, /* Capture */
	{8192, 6144, 000, 000, 8192, 6144, 2048, 1536,
	0000, 0000, 2048, 1536, 0, 0, 2048, 1536}, /* Video */
	{8192, 6144, 000, 000, 8192, 6144, 2048, 1536,
	0000, 0000, 2048, 1536, 0, 0, 2048, 1536}, /* hs_video */
	{8192, 6144, 000, 000, 8192, 6144, 2048, 1536,
	0000, 0000, 2048, 1536, 0, 0, 2048, 1536}, /* slim_video */
	{8192, 6144, 000, 000, 8192, 6144, 2048, 1536,
	0000, 0000, 2048, 1536, 0, 0, 2048, 1536},  /* custom1 */
	{8192, 6144, 000, 000, 8192, 6144, 2048, 1536,
	0000, 0000, 2048, 1536, 0, 0, 2048, 1536}, /* custom2 */
	{8192, 6144, 000, 000, 8192, 6144, 2048, 1536,
	0000, 0000, 2048, 1536, 0, 0, 2048, 1536}, /* custom3 */
};

//the index order of VC_STAGGER_NE/ME/SE in array identify the order of readout in MIPI transfer
static struct SENSOR_VC_INFO2_STRUCT SENSOR_VC_INFO2[6] = {
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,//preview
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x800, 0x600},
			{VC_PDAF_STATS_NE_PIX_1, 0x00, 0x30, 0x500, 0x180},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,//capture
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x800, 0x600},
			{VC_PDAF_STATS_NE_PIX_1, 0x00, 0x30, 0x500, 0x180},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,//video
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x800, 0x600},
			{VC_PDAF_STATS_NE_PIX_1, 0x00, 0x30, 0x500, 0x180},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,//custom1
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x800, 0x600},
			{VC_PDAF_STATS_NE_PIX_1, 0x00, 0x30, 0x500, 0x180},
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//custom2 2048x1536 2Exp
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x800, 0x600},
			{VC_STAGGER_ME, 0x01, 0x2b, 0x800, 0x600},
			{VC_PDAF_STATS_NE_PIX_1, 0x00, 0x30, 0x500, 0x180},
			{VC_PDAF_STATS_ME_PIX_1, 0x01, 0x30, 0x500, 0x180},
		},
		1
	},
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,//custom3 2048x1536 3Exp
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x800, 0x600},
			{VC_STAGGER_ME, 0x01, 0x2b, 0x800, 0x600},
			{VC_STAGGER_SE, 0x02, 0x2b, 0x800, 0x600},
			{VC_PDAF_STATS_NE_PIX_1, 0x00, 0x30, 0x500, 0x180},
			{VC_PDAF_STATS_ME_PIX_1, 0x01, 0x30, 0x500, 0x180},
			{VC_PDAF_STATS_SE_PIX_1, 0x02, 0x30, 0x500, 0x180},
		},
		1
	},
};

static kal_uint16 imx766dual_QSC_setting[3072 * 2];

static void get_vc_info_2(struct SENSOR_VC_INFO2_STRUCT *pvcinfo2, kal_uint32 scenario)
{
	switch (scenario) {
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[1],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[2],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[3],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[4],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[5],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	default:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	}
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte<<8)&0xff00) | ((get_byte>>8)&0x00ff);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
			(char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static kal_uint32 get_exp_cnt_by_scenario(kal_uint32 scenario)
{
	kal_uint32 exp_cnt = 0, i = 0;
	struct SENSOR_VC_INFO2_STRUCT vcinfo2;

	get_vc_info_2(&vcinfo2, scenario);

	for (i = 0; i < MAX_VC_INFO_CNT; ++i) {
		if (vcinfo2.vc_info[i].VC_FEATURE > VC_STAGGER_MIN_NUM &&
			vcinfo2.vc_info[i].VC_FEATURE < VC_STAGGER_MAX_NUM) {
			exp_cnt++;
		}
	}

	LOG_INF("%s exp_cnt %d\n", __func__, exp_cnt);
	return max(exp_cnt, (kal_uint32)1);
}

static kal_uint32 get_cur_exp_cnt(void)
{
	kal_uint32 exp_cnt = 1;

	if (0x1 == (read_cmos_sensor_8(0x33D0) & 0x1)) { // DOL_EN
		if (0x1 == (read_cmos_sensor_8(0x33D1) & 0x3)) { // DOL_MODE
			exp_cnt = 3;
		} else {
			exp_cnt = 2;
		}
	}

	return exp_cnt;
}

#if USE_BURST_MODE
#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */
static kal_uint16 imx766dual_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
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
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 3
			|| IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd,
						tosend,
						imgsensor.i2c_write_id,
						3,
						imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}
#else
#define I2C_BUFFER_LEN 765 /* trans# max is 255, each 3 bytes */
static kal_uint16 imx766dual_table_write_cmos_sensor(kal_uint16 *para,
						 kal_uint32 len)
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
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;
	}

	return 0;
}
#endif

static void imx766dual_get_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(regDa[idx]);
		/*LOG_INF("%x %x", regDa[idx], regDa[idx+1]);*/
	}
}
static void imx766dual_set_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	imx766dual_table_write_cmos_sensor(regDa, regNum*2);
}

static kal_uint16 read_cmos_eeprom_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA2);
	return get_byte;
}

static void read_sensor_Cali(void)
{
	kal_uint16 idx = 0, addr_qsc = 0x22c0, sensor_qsc = 0xc800;
	kal_uint8 otp_data[9] = {0};
	int i = 0;

	/*read otp data to distinguish module*/
	otp_flag = OTP_QSC_NONE;

	for (i = 0; i < 7; i++)
		otp_data[i] = read_cmos_eeprom_8(0x0006 + i);

	/*Internal Module Type*/
	if ((otp_data[0] == 0x05) &&
		(otp_data[1] == 0x00) &&
		(otp_data[2] == 0x47) &&
		(otp_data[3] == 0x01)) {
		LOG_INF("OTP type: Custom Only");
		otp_flag = OTP_QSC_CUSTOM;

		for (idx = 0; idx < 3072; idx++) {
			addr_qsc = 0x22c0 + idx;
			sensor_qsc = 0xc800 + idx;
			imx766dual_QSC_setting[2 * idx] = sensor_qsc;
			imx766dual_QSC_setting[2 * idx + 1] =
				read_cmos_eeprom_8(addr_qsc);
		}
	} else {
		LOG_INF("OTP type: No Data, 0x0008 = %d, 0x0009 = %d",
		read_cmos_eeprom_8(0x0008), read_cmos_eeprom_8(0x0009));
	}
}

static void write_sensor_QSC(void)
{
	// calibration tool version 3.0 -> 0x4E
	write_cmos_sensor_8(0x86A9, 0x4E);
	// set QSC from EEPROM to sensor
	if ((otp_flag == OTP_QSC_CUSTOM) || (otp_flag == OTP_QSC_INTERNAL)) {
		imx766dual_table_write_cmos_sensor(imx766dual_QSC_setting,
		sizeof(imx766dual_QSC_setting) / sizeof(kal_uint16));
	}
	write_cmos_sensor_8(0x32D2, 0x01);
}

static void write_frame_len(kal_uint32 fll)
{
	// //write_frame_len should be called inside GRP_PARAM_HOLD (0x0104)
	// FRM_LENGTH_LINES must be multiple of 4
	kal_uint32 exp_cnt = get_cur_exp_cnt();

	imgsensor.frame_length = round_up(fll / exp_cnt, 4) * exp_cnt;

	if (imgsensor.extend_frame_length_en == KAL_FALSE) {
		LOG_INF("fll %d exp_cnt %d\n", imgsensor.frame_length, exp_cnt);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length / exp_cnt >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length / exp_cnt & 0xFF);
	}

	if (imgsensor.fast_mode_on == KAL_TRUE) {
		imgsensor.fast_mode_on = KAL_FALSE;
		write_cmos_sensor_8(0x3010, 0x00);
	}
}

static void set_dummy(void)
{

	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	/* return;*/ /* for test */
	write_frame_len(imgsensor.frame_length);

}	/*	set_dummy  */

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

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n", framerate,
		min_framelength_en);

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

#define MAX_CIT_LSHIFT 7
static void write_shutter(kal_uint32 shutter, kal_bool gph)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 l_shift = 1;
	//30 + 24x  = 28 - 24x
	//subtract fine integ time custom1 2 line
	if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM1)
		shutter -= 2;

	shutter = round_up(shutter, 4);

	spin_lock(&imgsensor_drv_lock);
	// if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		// imgsensor.frame_length = shutter + imgsensor_info.margin;
	// else
	imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (gph)
		write_cmos_sensor_8(0x0104, 0x01);
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
				/ imgsensor.frame_length;
		LOG_INF("autoflicker enable, realtime_fps = %d\n",
			realtime_fps);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	}

	/* long expsoure */
	if (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) {

		for (l_shift = 1; l_shift < MAX_CIT_LSHIFT; l_shift++) {
			if ((shutter >> l_shift)
		    < (imgsensor_info.max_frame_length - imgsensor_info.margin))

				break;
		}
		if (l_shift > MAX_CIT_LSHIFT) {
			LOG_INF(
			    "Unable to set such a long exposure %d, set to max\n",
			    shutter);

			l_shift = MAX_CIT_LSHIFT;
		}
		shutter = shutter >> l_shift;
		// imgsensor.frame_length = shutter + imgsensor_info.margin;
		LOG_INF("enter long exposure mode, time is %d", l_shift);
		write_cmos_sensor_8(0x3100,
			read_cmos_sensor(0x3100) | (l_shift & 0x7));
		/* Frame exposure mode customization for LE*/
		imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		imgsensor.current_ae_effective_frame = 2;
	} else {
		write_cmos_sensor_8(0x3100, read_cmos_sensor(0x3100) & 0xf8);
		// write_frame_len(imgsensor.frame_length);
		imgsensor.current_ae_effective_frame = 2;
		LOG_INF("set frame_length\n");
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0350, 0x01); /* Enable auto extend */
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	if (gph)
		write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF("shutter =%d, framelength =%d\n",
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
static void set_shutter_w_gph(kal_uint32 shutter, kal_bool gph)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter, gph);
}
static void set_shutter(kal_uint32 shutter)
{
	set_shutter_w_gph(shutter, KAL_TRUE);
} /* set_shutter */

static void set_shutter_frame_length(
				kal_uint16 shutter, kal_uint16 frame_length,
				kal_bool auto_extend_en)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger
	 * than frame exposure
	 */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure
	 * lines must be updated here.
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
	// if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		// imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;

	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	write_cmos_sensor_8(0x0104, 0x01);
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	}
	/* Extend frame length */
	write_frame_len(imgsensor.frame_length);

	/* Update Shutter */
	if (auto_extend_en)
		write_cmos_sensor_8(0x0350, 0x01); /* Enable auto extend */
	else
		write_cmos_sensor_8(0x0350, 0x00); /* Disable auto extend */
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF(
	    "Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
	    shutter, imgsensor.frame_length,
	    frame_length, dummy_line,
	    read_cmos_sensor_8(0x0350));
}	/* set_shutter_frame_length */

#define IMX766DUAL_GAIN_TABL_SIZE 432
kal_uint16 imx766dual_gain_ratio[IMX766DUAL_GAIN_TABL_SIZE] = {
	64,
	65,
	66,
	67,
	68,
	69,
	70,
	71,
	72,
	73,
	74,
	75,
	76,
	77,
	78,
	79,
	80,
	81,
	82,
	83,
	84,
	85,
	86,
	87,
	88,
	89,
	90,
	91,
	92,
	93,
	94,
	95,
	96,
	97,
	98,
	99,
	100,
	101,
	102,
	103,
	104,
	105,
	106,
	107,
	108,
	109,
	110,
	111,
	112,
	113,
	114,
	115,
	116,
	117,
	118,
	119,
	120,
	121,
	122,
	123,
	124,
	125,
	126,
	127,
	128,
	129,
	130,
	131,
	132,
	133,
	134,
	135,
	136,
	137,
	138,
	139,
	140,
	141,
	142,
	143,
	144,
	145,
	146,
	147,
	148,
	149,
	150,
	151,
	152,
	153,
	154,
	155,
	156,
	157,
	158,
	159,
	160,
	161,
	162,
	163,
	164,
	165,
	166,
	167,
	168,
	169,
	170,
	171,
	172,
	173,
	174,
	175,
	176,
	177,
	178,
	179,
	180,
	181,
	182,
	183,
	184,
	185,
	186,
	187,
	188,
	189,
	190,
	191,
	192,
	193,
	194,
	195,
	196,
	197,
	198,
	199,
	200,
	201,
	202,
	203,
	204,
	205,
	206,
	207,
	208,
	209,
	210,
	211,
	212,
	213,
	214,
	215,
	216,
	217,
	218,
	219,
	220,
	221,
	222,
	223,
	224,
	225,
	226,
	227,
	228,
	229,
	230,
	231,
	232,
	233,
	234,
	235,
	236,
	237,
	238,
	239,
	240,
	241,
	242,
	243,
	244,
	245,
	246,
	247,
	248,
	249,
	250,
	251,
	252,
	253,
	254,
	255,
	256,
	257,
	258,
	259,
	260,
	261,
	262,
	263,
	264,
	265,
	266,
	267,
	268,
	269,
	270,
	271,
	273,
	274,
	275,
	276,
	277,
	278,
	280,
	281,
	282,
	283,
	284,
	286,
	287,
	288,
	289,
	291,
	292,
	293,
	295,
	296,
	297,
	299,
	300,
	302,
	303,
	304,
	306,
	307,
	309,
	310,
	312,
	313,
	315,
	318,
	319,
	321,
	322,
	324,
	326,
	327,
	329,
	330,
	332,
	334,
	336,
	337,
	339,
	341,
	343,
	344,
	346,
	348,
	350,
	352,
	354,
	356,
	358,
	360,
	362,
	364,
	366,
	368,
	370,
	372,
	374,
	376,
	378,
	381,
	383,
	385,
	387,
	390,
	392,
	394,
	397,
	399,
	402,
	404,
	407,
	409,
	412,
	414,
	417,
	420,
	422,
	425,
	428,
	431,
	434,
	436,
	439,
	442,
	445,
	448,
	451,
	455,
	458,
	461,
	464,
	468,
	471,
	474,
	478,
	481,
	485,
	489,
	492,
	496,
	500,
	504,
	508,
	512,
	516,
	520,
	524,
	528,
	532,
	537,
	541,
	546,
	550,
	555,
	560,
	564,
	569,
	574,
	579,
	585,
	590,
	595,
	601,
	606,
	612,
	618,
	624,
	630,
	636,
	642,
	648,
	655,
	661,
	668,
	675,
	682,
	689,
	697,
	704,
	712,
	720,
	728,
	736,
	744,
	753,
	762,
	771,
	780,
	789,
	799,
	809,
	819,
	829,
	840,
	851,
	862,
	873,
	885,
	897,
	910,
	923,
	936,
	949,
	963,
	978,
	992,
	1008,
	1024,
	1040,
	1057,
	1074,
	1092,
	1110,
	1129,
	1149,
	1170,
	1191,
	1213,
	1236,
	1260,
	1285,
	1310,
	1337,
	1365,
	1394,
	1424,
	1456,
	1489,
	1524,
	1560,
	1598,
	1638,
	1680,
	1724,
	1771,
	1820,
	1872,
	1927,
	1985,
	2048,
	2114,
	2184,
	2259,
	2340,
	2427,
	2520,
	2621,
	2730,
	2849,
	2978,
	3120,
	3276,
	3449,
	3640,
	3855,
	4096,
};
kal_uint16 imx766dual_gain_reg[IMX766DUAL_GAIN_TABL_SIZE] = {
	0,
	240,
	496,
	720,
	944,
	1168,
	1392,
	1616,
	1808,
	2000,
	2192,
	2384,
	2576,
	2768,
	2928,
	3088,
	3280,
	3408,
	3568,
	3728,
	3888,
	4048,
	4176,
	4304,
	4464,
	4592,
	4720,
	4848,
	4976,
	5104,
	5200,
	5328,
	5456,
	5552,
	5680,
	5776,
	5872,
	6000,
	6096,
	6192,
	6288,
	6384,
	6480,
	6576,
	6672,
	6736,
	6832,
	6928,
	7024,
	7088,
	7184,
	7248,
	7344,
	7408,
	7472,
	7568,
	7632,
	7696,
	7760,
	7856,
	7920,
	7984,
	8048,
	8112,
	8192,
	8240,
	8304,
	8368,
	8432,
	8496,
	8544,
	8608,
	8672,
	8720,
	8784,
	8832,
	8880,
	8944,
	8992,
	9040,
	9088,
	9152,
	9200,
	9248,
	9296,
	9344,
	9392,
	9440,
	9472,
	9520,
	9568,
	9616,
	9648,
	9696,
	9744,
	9776,
	9824,
	9856,
	9904,
	9936,
	9984,
	10016,
	10064,
	10096,
	10128,
	10176,
	10208,
	10240,
	10272,
	10320,
	10352,
	10384,
	10416,
	10448,
	10480,
	10512,
	10544,
	10576,
	10608,
	10640,
	10672,
	10704,
	10736,
	10768,
	10800,
	10832,
	10864,
	10880,
	10912,
	10944,
	10976,
	10992,
	11024,
	11056,
	11072,
	11104,
	11136,
	11152,
	11184,
	11216,
	11232,
	11264,
	11280,
	11312,
	11328,
	11360,
	11376,
	11408,
	11424,
	11456,
	11472,
	11504,
	11520,
	11536,
	11568,
	11584,
	11616,
	11632,
	11648,
	11680,
	11696,
	11712,
	11744,
	11760,
	11776,
	11792,
	11824,
	11840,
	11856,
	11872,
	11888,
	11920,
	11936,
	11952,
	11968,
	11984,
	12000,
	12032,
	12048,
	12064,
	12080,
	12096,
	12128,
	12144,
	12160,
	12176,
	12192,
	12208,
	12224,
	12240,
	12256,
	12272,
	12288,
	12304,
	12320,
	12336,
	12352,
	12368,
	12384,
	12400,
	12416,
	12432,
	12448,
	12464,
	12480,
	12496,
	12512,
	12528,
	12544,
	12560,
	12576,
	12592,
	12608,
	12624,
	12640,
	12656,
	12672,
	12688,
	12704,
	12720,
	12736,
	12752,
	12768,
	12784,
	12800,
	12816,
	12832,
	12848,
	12864,
	12880,
	12896,
	12912,
	12928,
	12944,
	12960,
	12976,
	12992,
	13008,
	13024,
	13040,
	13056,
	13088,
	13104,
	13120,
	13136,
	13152,
	13168,
	13184,
	13200,
	13216,
	13232,
	13248,
	13264,
	13280,
	13296,
	13312,
	13328,
	13344,
	13360,
	13376,
	13392,
	13408,
	13424,
	13440,
	13456,
	13472,
	13488,
	13504,
	13520,
	13536,
	13552,
	13568,
	13584,
	13600,
	13616,
	13632,
	13648,
	13664,
	13680,
	13696,
	13712,
	13728,
	13744,
	13760,
	13776,
	13792,
	13808,
	13824,
	13840,
	13856,
	13872,
	13888,
	13904,
	13920,
	13936,
	13952,
	13968,
	13984,
	14000,
	14016,
	14032,
	14048,
	14064,
	14080,
	14096,
	14112,
	14128,
	14144,
	14160,
	14176,
	14192,
	14208,
	14224,
	14240,
	14256,
	14272,
	14288,
	14304,
	14320,
	14336,
	14352,
	14368,
	14384,
	14400,
	14416,
	14432,
	14448,
	14464,
	14480,
	14496,
	14512,
	14528,
	14544,
	14560,
	14576,
	14592,
	14608,
	14624,
	14640,
	14656,
	14672,
	14688,
	14704,
	14720,
	14736,
	14752,
	14768,
	14784,
	14800,
	14816,
	14832,
	14848,
	14864,
	14880,
	14896,
	14912,
	14928,
	14944,
	14960,
	14976,
	14992,
	15008,
	15024,
	15040,
	15056,
	15072,
	15088,
	15104,
	15120,
	15136,
	15152,
	15168,
	15184,
	15200,
	15216,
	15232,
	15248,
	15264,
	15280,
	15296,
	15312,
	15328,
	15344,
	15360,
	15376,
	15392,
	15408,
	15424,
	15440,
	15456,
	15472,
	15488,
	15504,
	15520,
	15536,
	15552,
	15568,
	15584,
	15600,
	15616,
	15632,
	15648,
	15664,
	15680,
	15696,
	15712,
	15728,
	15744,
	15760,
	15776,
	15792,
	15808,
	15824,
	15840,
	15856,
	15872,
	15888,
	15904,
	15920,
	15936,
	15952,
	15968,
	15984,
	16000,
	16016,
	16032,
	16048,
	16064,
	16080,
	16096,
	16112,
	16128,
};

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	// 766 max gain: 64x
	kal_uint16 reg_gain = 0x0;
	kal_uint16 gain_value = gain;
#ifdef USE_GAIN_TABLE
	int i = 0;
#endif

	if (gain_value < imgsensor_info.min_gain || gain_value > imgsensor_info.max_gain) {
		LOG_INF("Error: gain value out of range %d", gain);

		if (gain_value < imgsensor_info.min_gain)
			gain_value = imgsensor_info.min_gain;
		else if (gain_value > imgsensor_info.max_gain)
			gain_value = imgsensor_info.max_gain;
	}

#ifdef USE_GAIN_TABLE
	reg_gain = imx766dual_gain_reg[IMX766DUAL_GAIN_TABL_SIZE - 1];
	for (i = 0; i < IMX766DUAL_GAIN_TABL_SIZE; i++) {
		if (gain_value <= imx766dual_gain_ratio[i]) {
			reg_gain = imx766dual_gain_reg[i];
			break;
		}
	}
#else
	reg_gain = 16384 - (16384*64)/gain_value;
#endif

	return (kal_uint16) reg_gain;
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
static kal_uint16 set_gain_w_gph(kal_uint16 gain, kal_bool gph)
{
	kal_uint16 reg_gain;

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d, reg_gain = 0x%x\n ", gain, reg_gain);

	if (gph)
		write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
	if (gph)
		write_cmos_sensor_8(0x0104, 0x00);

	return gain;
}
static kal_uint16 set_gain(kal_uint16 gain)
{
	return set_gain_w_gph(gain, KAL_TRUE);
} /* set_gain */

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
static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable)
		write_cmos_sensor_8(0x0100, 0X01);
	else
		write_cmos_sensor_8(0x0100, 0x00);
	return ERROR_NONE;
}

static kal_uint16 imx766dual_init_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x33F0, 0x02,
	0x33F1, 0x05,
	0x0111, 0x03,
	0x1F04, 0xB3,
	0x1F05, 0x01,
	0x1F06, 0x07,
	0x1F07, 0x66,
	0x1F08, 0x01,
	0x33F2, 0x01,
	0x1F04, 0xA3,
	0x1F05, 0x01,
	0x0600, 0x00,
	0x0601, 0x02,
	0x3877, 0x01,
	0xB500, 0x00,
	0x33F2, 0x02,
	0x1F08, 0x00,
	0x0101, 0x03,
	0x33D3, 0x01,
	0x3892, 0x01,
	0x4C14, 0x00,
	0x4C15, 0x07,
	0x4C16, 0x00,
	0x4C17, 0x1B,
	0x4C1A, 0x00,
	0x4C1B, 0x03,
	0x4C1C, 0x00,
	0x4C1D, 0x00,
	0x4C1E, 0x00,
	0x4C1F, 0x02,
	0x4C20, 0x00,
	0x4C21, 0x5F,
	0x4C26, 0x00,
	0x4C27, 0x43,
	0x4C28, 0x00,
	0x4C29, 0x09,
	0x4C2A, 0x00,
	0x4C2B, 0x4A,
	0x4C2C, 0x00,
	0x4C2D, 0x00,
	0x4C2E, 0x00,
	0x4C2F, 0x02,
	0x4C30, 0x00,
	0x4C31, 0xC6,
	0x4C3E, 0x00,
	0x4C3F, 0x55,
	0x4C52, 0x00,
	0x4C53, 0x97,
	0x4CB4, 0x00,
	0x4CB5, 0x55,
	0x4CC8, 0x00,
	0x4CC9, 0x97,
	0x4D04, 0x00,
	0x4D05, 0x4F,
	0x4D18, 0x00,
	0x4D19, 0x9D,
	0x4D74, 0x00,
	0x4D75, 0x55,
	0x4D88, 0x00,
	0x4D89, 0x97,
	0x4F06, 0x00,
	0x4F07, 0x5F,
	0x4F48, 0x00,
	0x4F49, 0xC6,
	0x544A, 0xFF,
	0x544B, 0xFF,
	0x544E, 0x01,
	0x544F, 0xBD,
	0x5452, 0xFF,
	0x5453, 0xFF,
	0x5456, 0x00,
	0x5457, 0xA5,
	0x545A, 0xFF,
	0x545B, 0xFF,
	0x545E, 0x00,
	0x545F, 0xA5,
	0x5496, 0x00,
	0x5497, 0xA2,
	0x54F6, 0x01,
	0x54F7, 0x55,
	0x54F8, 0x01,
	0x54F9, 0x61,
	0x5670, 0x00,
	0x5671, 0x85,
	0x5672, 0x01,
	0x5673, 0x77,
	0x5674, 0x01,
	0x5675, 0x2F,
	0x5676, 0x02,
	0x5677, 0x55,
	0x5678, 0x00,
	0x5679, 0x85,
	0x567A, 0x01,
	0x567B, 0x77,
	0x567C, 0x01,
	0x567D, 0x2F,
	0x567E, 0x02,
	0x567F, 0x55,
	0x5680, 0x00,
	0x5681, 0x85,
	0x5682, 0x01,
	0x5683, 0x77,
	0x5684, 0x01,
	0x5685, 0x2F,
	0x5686, 0x02,
	0x5687, 0x55,
	0x5688, 0x00,
	0x5689, 0x85,
	0x568A, 0x01,
	0x568B, 0x77,
	0x568C, 0x01,
	0x568D, 0x2F,
	0x568E, 0x02,
	0x568F, 0x55,
	0x5690, 0x01,
	0x5691, 0x7A,
	0x5692, 0x02,
	0x5693, 0x6C,
	0x5694, 0x01,
	0x5695, 0x35,
	0x5696, 0x02,
	0x5697, 0x5B,
	0x5698, 0x01,
	0x5699, 0x7A,
	0x569A, 0x02,
	0x569B, 0x6C,
	0x569C, 0x01,
	0x569D, 0x35,
	0x569E, 0x02,
	0x569F, 0x5B,
	0x56A0, 0x01,
	0x56A1, 0x7A,
	0x56A2, 0x02,
	0x56A3, 0x6C,
	0x56A4, 0x01,
	0x56A5, 0x35,
	0x56A6, 0x02,
	0x56A7, 0x5B,
	0x56A8, 0x01,
	0x56A9, 0x80,
	0x56AA, 0x02,
	0x56AB, 0x72,
	0x56AC, 0x01,
	0x56AD, 0x2F,
	0x56AE, 0x02,
	0x56AF, 0x55,
	0x5902, 0x0E,
	0x5A50, 0x04,
	0x5A51, 0x04,
	0x5A69, 0x01,
	0x5C49, 0x0D,
	0x5C57, 0x0A,
	0x5D60, 0x08,
	0x5D61, 0x08,
	0x5D62, 0x08,
	0x5D63, 0x08,
	0x5D64, 0x08,
	0x5D67, 0x08,
	0x5D6C, 0x08,
	0x5D6E, 0x08,
	0x5D71, 0x08,
	0x5D8E, 0x14,
	0x5D90, 0x03,
	0x5D91, 0x0A,
	0x5D92, 0x1F,
	0x5D93, 0x05,
	0x5D94, 0x1F,
	0x5D97, 0x1F,
	0x5D9A, 0x06,
	0x5D9C, 0x1F,
	0x5D9E, 0x1F,
	0x5DA1, 0x1F,
	0x5DA6, 0x1F,
	0x5DA8, 0x1F,
	0x5DAB, 0x1F,
	0x5DC0, 0x06,
	0x5DC1, 0x06,
	0x5DC2, 0x07,
	0x5DC3, 0x06,
	0x5DC4, 0x07,
	0x5DC7, 0x07,
	0x5DCC, 0x07,
	0x5DCE, 0x07,
	0x5DD1, 0x07,
	0x5E3E, 0x00,
	0x5E3F, 0x00,
	0x5E41, 0x00,
	0x5E48, 0x00,
	0x5E49, 0x00,
	0x5E4A, 0x00,
	0x5E4C, 0x00,
	0x5E4D, 0x00,
	0x5E4E, 0x00,
	0x5E50, 0x23,
	0x5E51, 0x20,
	0x5E52, 0x07,
	0x5E53, 0x20,
	0x5E54, 0x07,
	0x5E55, 0x27,
	0x5E56, 0x0B,
	0x5E57, 0x24,
	0x5E58, 0x0B,
	0x5E60, 0x24,
	0x5E61, 0x24,
	0x5E62, 0x1B,
	0x5E63, 0x23,
	0x5E64, 0x1B,
	0x5E65, 0x28,
	0x5E66, 0x22,
	0x5E67, 0x28,
	0x5E68, 0x23,
	0x5E70, 0x25,
	0x5E71, 0x24,
	0x5E72, 0x20,
	0x5E73, 0x24,
	0x5E74, 0x20,
	0x5E75, 0x28,
	0x5E76, 0x27,
	0x5E77, 0x29,
	0x5E78, 0x24,
	0x5E80, 0x25,
	0x5E81, 0x25,
	0x5E82, 0x24,
	0x5E83, 0x25,
	0x5E84, 0x23,
	0x5E85, 0x2A,
	0x5E86, 0x28,
	0x5E87, 0x2A,
	0x5E88, 0x28,
	0x5E90, 0x24,
	0x5E91, 0x24,
	0x5E92, 0x28,
	0x5E93, 0x29,
	0x5E97, 0x25,
	0x5E98, 0x25,
	0x5E99, 0x2A,
	0x5E9A, 0x2A,
	0x5E9E, 0x3A,
	0x5E9F, 0x3F,
	0x5EA0, 0x17,
	0x5EA1, 0x3F,
	0x5EA2, 0x17,
	0x5EA3, 0x32,
	0x5EA4, 0x10,
	0x5EA5, 0x33,
	0x5EA6, 0x10,
	0x5EAE, 0x3D,
	0x5EAF, 0x48,
	0x5EB0, 0x3B,
	0x5EB1, 0x45,
	0x5EB2, 0x37,
	0x5EB3, 0x3A,
	0x5EB4, 0x31,
	0x5EB5, 0x3A,
	0x5EB6, 0x31,
	0x5EBE, 0x40,
	0x5EBF, 0x48,
	0x5EC0, 0x3F,
	0x5EC1, 0x45,
	0x5EC2, 0x3F,
	0x5EC3, 0x3A,
	0x5EC4, 0x32,
	0x5EC5, 0x3A,
	0x5EC6, 0x33,
	0x5ECE, 0x4B,
	0x5ECF, 0x4A,
	0x5ED0, 0x48,
	0x5ED1, 0x4C,
	0x5ED2, 0x45,
	0x5ED3, 0x3F,
	0x5ED4, 0x3A,
	0x5ED5, 0x3F,
	0x5ED6, 0x3A,
	0x5EDE, 0x48,
	0x5EDF, 0x45,
	0x5EE0, 0x3A,
	0x5EE1, 0x3A,
	0x5EE5, 0x4A,
	0x5EE6, 0x4C,
	0x5EE7, 0x3F,
	0x5EE8, 0x3F,
	0x5EEC, 0x06,
	0x5EED, 0x06,
	0x5EEE, 0x02,
	0x5EEF, 0x06,
	0x5EF0, 0x01,
	0x5EF1, 0x09,
	0x5EF2, 0x05,
	0x5EF3, 0x06,
	0x5EF4, 0x04,
	0x5EFC, 0x07,
	0x5EFD, 0x09,
	0x5EFE, 0x05,
	0x5EFF, 0x08,
	0x5F00, 0x04,
	0x5F01, 0x09,
	0x5F02, 0x05,
	0x5F03, 0x09,
	0x5F04, 0x04,
	0x5F0C, 0x08,
	0x5F0D, 0x09,
	0x5F0E, 0x06,
	0x5F0F, 0x09,
	0x5F10, 0x06,
	0x5F11, 0x09,
	0x5F12, 0x09,
	0x5F13, 0x09,
	0x5F14, 0x06,
	0x5F1C, 0x09,
	0x5F1D, 0x09,
	0x5F1E, 0x09,
	0x5F1F, 0x09,
	0x5F20, 0x08,
	0x5F21, 0x09,
	0x5F22, 0x09,
	0x5F23, 0x09,
	0x5F24, 0x09,
	0x5F2C, 0x09,
	0x5F2D, 0x09,
	0x5F2E, 0x09,
	0x5F2F, 0x09,
	0x5F33, 0x09,
	0x5F34, 0x09,
	0x5F35, 0x09,
	0x5F36, 0x09,
	0x5F3A, 0x01,
	0x5F3D, 0x07,
	0x5F3F, 0x01,
	0x5F4B, 0x01,
	0x5F4D, 0x04,
	0x5F4F, 0x02,
	0x5F51, 0x02,
	0x5F5A, 0x02,
	0x5F5B, 0x01,
	0x5F5D, 0x03,
	0x5F5E, 0x07,
	0x5F5F, 0x01,
	0x5F60, 0x01,
	0x5F61, 0x01,
	0x5F6A, 0x01,
	0x5F6C, 0x01,
	0x5F6D, 0x01,
	0x5F6E, 0x04,
	0x5F70, 0x02,
	0x5F72, 0x02,
	0x5F7A, 0x01,
	0x5F7B, 0x03,
	0x5F7C, 0x01,
	0x5F7D, 0x01,
	0x5F82, 0x01,
	0x6026, 0x03,
	0x6028, 0x03,
	0x602A, 0x03,
	0x602C, 0x03,
	0x602F, 0x03,
	0x6036, 0x03,
	0x6038, 0x03,
	0x603A, 0x03,
	0x603C, 0x03,
	0x603F, 0x03,
	0x6074, 0x19,
	0x6076, 0x19,
	0x6078, 0x19,
	0x607A, 0x19,
	0x607D, 0x19,
	0x6084, 0x32,
	0x6086, 0x32,
	0x6088, 0x32,
	0x608A, 0x32,
	0x608D, 0x32,
	0x60C2, 0x4A,
	0x60C4, 0x4A,
	0x60C6, 0x4A,
	0x60C8, 0x4A,
	0x60CB, 0x4A,
	0x60D2, 0x4A,
	0x60D4, 0x4A,
	0x60D6, 0x4A,
	0x60D8, 0x4A,
	0x60DB, 0x4A,
	0x62E4, 0x33,
	0x62E9, 0x33,
	0x62EE, 0x1C,
	0x62EF, 0x33,
	0x62F3, 0x33,
	0x62F6, 0x1C,
	0x62F9, 0x14,
	0x6305, 0x13,
	0x6307, 0x13,
	0x630A, 0x13,
	0x630D, 0x0D,
	0x6317, 0x0D,
	0x632F, 0x2E,
	0x6333, 0x2E,
	0x6339, 0x2E,
	0x6343, 0x2E,
	0x6347, 0x2E,
	0x634D, 0x2E,
	0x6352, 0x00,
	0x6353, 0x5F,
	0x6366, 0x00,
	0x6367, 0x5F,
	0x638F, 0x95,
	0x6393, 0x95,
	0x6399, 0x95,
	0x63A3, 0x95,
	0x63A7, 0x95,
	0x63AD, 0x95,
	0x63B2, 0x00,
	0x63B3, 0xC6,
	0x63C6, 0x00,
	0x63C7, 0xC6,
	0x8BDB, 0x02,
	0x8BDE, 0x02,
	0x8BE1, 0x2D,
	0x8BE4, 0x00,
	0x8BE5, 0x00,
	0x8BE6, 0x01,
	0x9002, 0x14,
	0x9200, 0xB5,
	0x9201, 0x9E,
	0x9202, 0xB5,
	0x9203, 0x42,
	0x9204, 0xB5,
	0x9205, 0x43,
	0x9206, 0xBD,
	0x9207, 0x20,
	0x9208, 0xBD,
	0x9209, 0x22,
	0x920A, 0xBD,
	0x920B, 0x23,
	0x920C, 0xFF,
	0x920D, 0xFF,
	0x920E, 0xFF,
	0x920F, 0xFF,
	0x9210, 0xFF,
	0x9211, 0xFF,
	0x9212, 0xFF,
	0x9213, 0xFF,
	0x9214, 0xFF,
	0x9215, 0xFF,
	0x9216, 0xFF,
	0x9217, 0xFF,
	0x9218, 0xFF,
	0x9219, 0xFF,
	0x921A, 0xFF,
	0x921B, 0xFF,
	0x921C, 0xFF,
	0x921D, 0xFF,
	0x921E, 0xFF,
	0x921F, 0xFF,
	0x9220, 0xFF,
	0x9221, 0xFF,
	0x9222, 0xFF,
	0x9223, 0xFF,
	0x9224, 0xFF,
	0x9225, 0xFF,
	0x9226, 0xFF,
	0x9227, 0xFF,
	0xB5D7, 0x10,
	0xBD24, 0x00,
	0xBD25, 0x00,
	0xBD26, 0x00,
	0xBD27, 0x00,
	0xBD28, 0x00,
	0xBD29, 0x00,
	0xBD2A, 0x00,
	0xBD2B, 0x00,
	0xBD2C, 0x32,
	0xBD2D, 0x70,
	0xBD2E, 0x25,
	0xBD2F, 0x30,
	0xBD30, 0x3B,
	0xBD31, 0xE0,
	0xBD32, 0x69,
	0xBD33, 0x40,
	0xBD34, 0x25,
	0xBD35, 0x90,
	0xBD36, 0x58,
	0xBD37, 0x00,
	0xBD38, 0x00,
	0xBD39, 0x00,
	0xBD3A, 0x00,
	0xBD3B, 0x00,
	0xBD3C, 0x32,
	0xBD3D, 0x70,
	0xBD3E, 0x25,
	0xBD3F, 0x90,
	0xBD40, 0x58,
	0xBD41, 0x00,
	0x793B, 0x01,
	0xACC6, 0x00,
	0xACF5, 0x00,
	0x793B, 0x00,
	0x32D2, 0x00,
};

static kal_uint16 imx766dual_capture_30_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x22,
	0x0343, 0x70,
	0x0340, 0x12,
	0x0341, 0xEC,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x08,
	0x3005, 0x03,
	0x3120, 0x04,
	0x3121, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x32D6, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x08,
	0x040D, 0x00,
	0x040E, 0x06,
	0x040F, 0x00,
	0x034C, 0x08,
	0x034D, 0x00,
	0x034E, 0x06,
	0x034F, 0x00,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x0B,
	0x030B, 0x04,
	0x030D, 0x03,
	0x030E, 0x01,
	0x030F, 0x74,
	0x30CB, 0x00,
	0x30CC, 0x04,
	0x30CD, 0x00,
	0x30CE, 0x01,
	0x30CF, 0x80,
	0x319C, 0x01,
	0x3800, 0x01,
	0x3801, 0x01,
	0x3802, 0x02,
	0x3847, 0x03,
	0x38B0, 0x00,
	0x38B1, 0x64,
	0x38B2, 0x00,
	0x38B3, 0x64,
	0x38C4, 0x00,
	0x38C5, 0x64,
	0x4C3A, 0x02,
	0x4C3B, 0xD2,
	0x4C68, 0x04,
	0x4C69, 0x7E,
	0x4CF8, 0x07,
	0x4CF9, 0x9E,
	0x4DB8, 0x08,
	0x4DB9, 0x98,
	0x0202, 0x12,
	0x0203, 0xBC,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x313A, 0x01,
	0x313B, 0xF4,
	0x3803, 0x00,
	0x3804, 0x17,
	0x3805, 0xC0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x313C, 0x00,
	0x313D, 0x00,
	0x313E, 0x01,
	0x313F, 0x00,
	0x0860, 0x01,
	0x0861, 0x2D,
	0x0862, 0x01,
	0x0863, 0x2D,
	0x30B4, 0x01,
	0x3066, 0x00,
	0x3067, 0x30,
	0x3068, 0x00,
	0x3069, 0x31,
	0x33D0, 0x00,
	0x33D1, 0x00,
	0x33D4, 0x01,
	0x33DC, 0x0A,
	0x33DD, 0x0A,
	0x33DE, 0x0A,
	0x33DF, 0x0A,
	0x3070, 0x01,
	0x3077, 0x01,
	0x3078, 0x30,
	0x3079, 0x01,
	0x307A, 0x31,
	0x307B, 0x01,
	0x3080, 0x02,
	0x3087, 0x02,
	0x3088, 0x30,
	0x3089, 0x02,
	0x308A, 0x31,
	0x308B, 0x02,
	0x3901, 0x2B,
	0x3902, 0x00,
	0x3903, 0x12,
	0x3905, 0x2B,
	0x3906, 0x01,
	0x3907, 0x12,
	0x3909, 0x2B,
	0x390A, 0x02,
	0x390B, 0x12,
	0x3911, 0x00,
};

static kal_uint16 imx766dual_preview_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x22,
	0x0343, 0x70,
	0x0340, 0x12,
	0x0341, 0xEC,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x08,
	0x3005, 0x03,
	0x3120, 0x04,
	0x3121, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x32D6, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x08,
	0x040D, 0x00,
	0x040E, 0x06,
	0x040F, 0x00,
	0x034C, 0x08,
	0x034D, 0x00,
	0x034E, 0x06,
	0x034F, 0x00,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x0B,
	0x030B, 0x04,
	0x030D, 0x03,
	0x030E, 0x01,
	0x030F, 0x74,
	0x30CB, 0x00,
	0x30CC, 0x04,
	0x30CD, 0x00,
	0x30CE, 0x01,
	0x30CF, 0x80,
	0x319C, 0x01,
	0x3800, 0x01,
	0x3801, 0x01,
	0x3802, 0x02,
	0x3847, 0x03,
	0x38B0, 0x00,
	0x38B1, 0x64,
	0x38B2, 0x00,
	0x38B3, 0x64,
	0x38C4, 0x00,
	0x38C5, 0x64,
	0x4C3A, 0x02,
	0x4C3B, 0xD2,
	0x4C68, 0x04,
	0x4C69, 0x7E,
	0x4CF8, 0x07,
	0x4CF9, 0x9E,
	0x4DB8, 0x08,
	0x4DB9, 0x98,
	0x0202, 0x12,
	0x0203, 0xBC,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x313A, 0x01,
	0x313B, 0xF4,
	0x3803, 0x00,
	0x3804, 0x17,
	0x3805, 0xC0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x313C, 0x00,
	0x313D, 0x00,
	0x313E, 0x01,
	0x313F, 0x00,
	0x0860, 0x01,
	0x0861, 0x2D,
	0x0862, 0x01,
	0x0863, 0x2D,
	0x30B4, 0x01,
	0x3066, 0x00,
	0x3067, 0x30,
	0x3068, 0x00,
	0x3069, 0x31,
	0x33D0, 0x00,
	0x33D1, 0x00,
	0x33D4, 0x01,
	0x33DC, 0x0A,
	0x33DD, 0x0A,
	0x33DE, 0x0A,
	0x33DF, 0x0A,
	0x3070, 0x01,
	0x3077, 0x01,
	0x3078, 0x30,
	0x3079, 0x01,
	0x307A, 0x31,
	0x307B, 0x01,
	0x3080, 0x02,
	0x3087, 0x02,
	0x3088, 0x30,
	0x3089, 0x02,
	0x308A, 0x31,
	0x308B, 0x02,
	0x3901, 0x2B,
	0x3902, 0x00,
	0x3903, 0x12,
	0x3905, 0x2B,
	0x3906, 0x01,
	0x3907, 0x12,
	0x3909, 0x2B,
	0x390A, 0x02,
	0x390B, 0x12,
	0x3911, 0x00,
};


// NOTE:
// for 2 exp setting,  VCID of LE/SE should be 0x00 and 0x02
// which align 3 exp setting LE/NE/SE 0x00,  0x01,  0x02
// to seamless switch,  VC ID of SE should remain the same
// SONY sensor: VCID of 2nd frame at 0x3070; VCID of 3rd frame at 0x3080
// must be two different value
static kal_uint16 imx766dual_custom1_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x22,
	0x0343, 0x70,
	0x0340, 0x12,
	0x0341, 0xEC,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3005, 0x03,
	0x3120, 0x04,
	0x3121, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x32D6, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x08,
	0x040D, 0x00,
	0x040E, 0x06,
	0x040F, 0x00,
	0x034C, 0x08,
	0x034D, 0x00,
	0x034E, 0x06,
	0x034F, 0x00,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x0B,
	0x030B, 0x04,
	0x030D, 0x03,
	0x030E, 0x01,
	0x030F, 0x74,
	0x30CB, 0x00,
	0x30CC, 0x04,
	0x30CD, 0x00,
	0x30CE, 0x01,
	0x30CF, 0x80,
	0x319C, 0x01,
	0x3800, 0x01,
	0x3801, 0x01,
	0x3802, 0x02,
	0x3847, 0x03,
	0x38B0, 0x00,
	0x38B1, 0x64,
	0x38B2, 0x00,
	0x38B3, 0x64,
	0x38C4, 0x00,
	0x38C5, 0x64,
	0x4C3A, 0x02,
	0x4C3B, 0xD2,
	0x4C68, 0x04,
	0x4C69, 0x7E,
	0x4CF8, 0x07,
	0x4CF9, 0x9E,
	0x4DB8, 0x08,
	0x4DB9, 0x98,
	0x0202, 0x12,
	0x0203, 0xBC,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x313A, 0x01,
	0x313B, 0xF4,
	0x3803, 0x00,
	0x3804, 0x17,
	0x3805, 0xC0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x313C, 0x00,
	0x313D, 0x00,
	0x313E, 0x01,
	0x313F, 0x00,
	0x0860, 0x01,
	0x0861, 0x2D,
	0x0862, 0x01,
	0x0863, 0x2D,
	0x30B4, 0x01,
	0x3066, 0x00,
	0x3067, 0x30,
	0x3068, 0x00,
	0x3069, 0x31,
	0x33D0, 0x00,
	0x33D1, 0x00,
	0x33D4, 0x01,
	0x33DC, 0x0A,
	0x33DD, 0x0A,
	0x33DE, 0x0A,
	0x33DF, 0x0A,
	0x3070, 0x01,
	0x3077, 0x01,
	0x3078, 0x30,
	0x3079, 0x01,
	0x307A, 0x31,
	0x307B, 0x01,
	0x3080, 0x02,
	0x3087, 0x02,
	0x3088, 0x30,
	0x3089, 0x02,
	0x308A, 0x31,
	0x308B, 0x02,
	0x3901, 0x2B,
	0x3902, 0x00,
	0x3903, 0x12,
	0x3905, 0x2B,
	0x3906, 0x01,
	0x3907, 0x12,
	0x3909, 0x2B,
	0x390A, 0x02,
	0x390B, 0x12,
	0x3911, 0x00,
};

static kal_uint16 imx766dual_normal_video_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x22,
	0x0343, 0x70,
	0x0340, 0x12,
	0x0341, 0xEC,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x08,
	0x3005, 0x03,
	0x3120, 0x04,
	0x3121, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x32D6, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x08,
	0x040D, 0x00,
	0x040E, 0x06,
	0x040F, 0x00,
	0x034C, 0x08,
	0x034D, 0x00,
	0x034E, 0x06,
	0x034F, 0x00,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x0B,
	0x030B, 0x04,
	0x030D, 0x03,
	0x030E, 0x01,
	0x030F, 0x74,
	0x30CB, 0x00,
	0x30CC, 0x04,
	0x30CD, 0x00,
	0x30CE, 0x01,
	0x30CF, 0x80,
	0x319C, 0x01,
	0x3800, 0x01,
	0x3801, 0x01,
	0x3802, 0x02,
	0x3847, 0x03,
	0x38B0, 0x00,
	0x38B1, 0x64,
	0x38B2, 0x00,
	0x38B3, 0x64,
	0x38C4, 0x00,
	0x38C5, 0x64,
	0x4C3A, 0x02,
	0x4C3B, 0xD2,
	0x4C68, 0x04,
	0x4C69, 0x7E,
	0x4CF8, 0x07,
	0x4CF9, 0x9E,
	0x4DB8, 0x08,
	0x4DB9, 0x98,
	0x0202, 0x12,
	0x0203, 0xBC,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x313A, 0x01,
	0x313B, 0xF4,
	0x3803, 0x00,
	0x3804, 0x17,
	0x3805, 0xC0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x313C, 0x00,
	0x313D, 0x00,
	0x313E, 0x01,
	0x313F, 0x00,
	0x0860, 0x01,
	0x0861, 0x2D,
	0x0862, 0x01,
	0x0863, 0x2D,
	0x30B4, 0x01,
	0x3066, 0x00,
	0x3067, 0x30,
	0x3068, 0x00,
	0x3069, 0x31,
	0x33D0, 0x00,
	0x33D1, 0x00,
	0x33D4, 0x01,
	0x33DC, 0x0A,
	0x33DD, 0x0A,
	0x33DE, 0x0A,
	0x33DF, 0x0A,
	0x3070, 0x01,
	0x3077, 0x01,
	0x3078, 0x30,
	0x3079, 0x01,
	0x307A, 0x31,
	0x307B, 0x01,
	0x3080, 0x02,
	0x3087, 0x02,
	0x3088, 0x30,
	0x3089, 0x02,
	0x308A, 0x31,
	0x308B, 0x02,
	0x3901, 0x2B,
	0x3902, 0x00,
	0x3903, 0x12,
	0x3905, 0x2B,
	0x3906, 0x01,
	0x3907, 0x12,
	0x3909, 0x2B,
	0x390A, 0x02,
	0x390B, 0x12,
	0x3911, 0x00,
};

// NOTE:
// for 2 exp setting,  VCID of LE/SE should be 0x00 and 0x02
// which align 3 exp setting LE/NE/SE 0x00,  0x01,  0x02
// to seamless switch,  VC ID of SE should remain the same
// SONY sensor: VCID of 2nd frame at 0x3070; VCID of 3rd frame at 0x3080
// must be two different value
static kal_uint16 imx766dual_custom2_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x22,
	0x0343, 0x70,
	0x0340, 0x09,
	0x0341, 0x74,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x08,
	0x3005, 0x03,
	0x3120, 0x04,
	0x3121, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x32D6, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x08,
	0x040D, 0x00,
	0x040E, 0x06,
	0x040F, 0x00,
	0x034C, 0x08,
	0x034D, 0x00,
	0x034E, 0x06,
	0x034F, 0x00,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x0B,
	0x030B, 0x04,
	0x030D, 0x03,
	0x030E, 0x01,
	0x030F, 0x74,
	0x30CB, 0x00,
	0x30CC, 0x04,
	0x30CD, 0x00,
	0x30CE, 0x01,
	0x30CF, 0x80,
	0x319C, 0x01,
	0x3800, 0x01,
	0x3801, 0x01,
	0x3802, 0x02,
	0x3847, 0x03,
	0x38B0, 0x00,
	0x38B1, 0x64,
	0x38B2, 0x00,
	0x38B3, 0x64,
	0x38C4, 0x00,
	0x38C5, 0x64,
	0x4C3A, 0x02,
	0x4C3B, 0xD2,
	0x4C68, 0x04,
	0x4C69, 0x7E,
	0x4CF8, 0x07,
	0x4CF9, 0x9E,
	0x4DB8, 0x08,
	0x4DB9, 0x98,
	0x0202, 0x08,
	0x0203, 0x00,
	0x0224, 0x01,
	0x0225, 0x00,
	0x313A, 0x01,
	0x313B, 0xF4,
	0x3803, 0x00,
	0x3804, 0x17,
	0x3805, 0xC0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x313C, 0x00,
	0x313D, 0x00,
	0x313E, 0x01,
	0x313F, 0x00,
	0x0860, 0x01,
	0x0861, 0x2D,
	0x0862, 0x01,
	0x0863, 0x2D,
	0x30B4, 0x03,
	0x3066, 0x00,
	0x3067, 0x30,
	0x3068, 0x00,
	0x3069, 0x31,
	0x33D0, 0x01,
	0x33D1, 0x00,
	0x33D4, 0x01,
	0x33DC, 0x0A,
	0x33DD, 0x0A,
	0x33DE, 0x0A,
	0x33DF, 0x0A,
	0x3070, 0x01,
	0x3077, 0x01,
	0x3078, 0x30,
	0x3079, 0x01,
	0x307A, 0x31,
	0x307B, 0x01,
	0x3080, 0x02,
	0x3087, 0x02,
	0x3088, 0x30,
	0x3089, 0x02,
	0x308A, 0x31,
	0x308B, 0x02,
	0x3901, 0x2B,
	0x3902, 0x00,
	0x3903, 0x12,
	0x3905, 0x2B,
	0x3906, 0x01,
	0x3907, 0x12,
	0x3909, 0x2B,
	0x390A, 0x02,
	0x390B, 0x12,
	0x3911, 0x00,
};

static kal_uint16 imx766dual_custom3_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	0x0342, 0x22,
	0x0343, 0x70,
	0x0340, 0x06,
	0x0341, 0x4C,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x08,
	0x3005, 0x03,
	0x3120, 0x04,
	0x3121, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x32D6, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x08,
	0x040D, 0x00,
	0x040E, 0x06,
	0x040F, 0x00,
	0x034C, 0x08,
	0x034D, 0x00,
	0x034E, 0x06,
	0x034F, 0x00,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x0B,
	0x030B, 0x04,
	0x030D, 0x03,
	0x030E, 0x01,
	0x030F, 0x74,
	0x30CB, 0x00,
	0x30CC, 0x04,
	0x30CD, 0x00,
	0x30CE, 0x01,
	0x30CF, 0x80,
	0x319C, 0x01,
	0x3800, 0x01,
	0x3801, 0x01,
	0x3802, 0x02,
	0x3847, 0x03,
	0x38B0, 0x00,
	0x38B1, 0x64,
	0x38B2, 0x00,
	0x38B3, 0x64,
	0x38C4, 0x00,
	0x38C5, 0x64,
	0x4C3A, 0x02,
	0x4C3B, 0xD2,
	0x4C68, 0x04,
	0x4C69, 0x7E,
	0x4CF8, 0x07,
	0x4CF9, 0x9E,
	0x4DB8, 0x08,
	0x4DB9, 0x98,
	0x0202, 0x05,
	0x0203, 0x00,
	0x0224, 0x00,
	0x0225, 0x14,
	0x313A, 0x00,
	0x313B, 0xA0,
	0x3803, 0x00,
	0x3804, 0x17,
	0x3805, 0xC0,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x313C, 0x00,
	0x313D, 0x00,
	0x313E, 0x01,
	0x313F, 0x00,
	0x0860, 0x01,
	0x0861, 0x2D,
	0x0862, 0x01,
	0x0863, 0x2D,
	0x30B4, 0x07,
	0x3066, 0x00,
	0x3067, 0x30,
	0x3068, 0x00,
	0x3069, 0x31,
	0x33D0, 0x01,
	0x33D1, 0x01,
	0x33D4, 0x01,
	0x33DC, 0x0A,
	0x33DD, 0x0A,
	0x33DE, 0x0A,
	0x33DF, 0x0A,
	0x3070, 0x01,
	0x3077, 0x01,
	0x3078, 0x30,
	0x3079, 0x01,
	0x307A, 0x31,
	0x307B, 0x01,
	0x3080, 0x02,
	0x3087, 0x02,
	0x3088, 0x30,
	0x3089, 0x02,
	0x308A, 0x31,
	0x308B, 0x02,
	0x3901, 0x2B,
	0x3902, 0x00,
	0x3903, 0x12,
	0x3905, 0x2B,
	0x3906, 0x01,
	0x3907, 0x12,
	0x3909, 0x2B,
	0x390A, 0x02,
	0x390B, 0x12,
	0x3911, 0x00,
};

static void extend_frame_length(kal_uint32 ns)
{
	UINT32 old_fl = imgsensor.frame_length;

	kal_uint32 per_frame_ms =
		(kal_uint32)(((unsigned long long)imgsensor.frame_length *
		(unsigned long long)imgsensor.line_length * 1000) /
		(unsigned long long)imgsensor.pclk);

	LOG_INF("per_frame_ms: %d / %d = %d",
		(imgsensor.frame_length * imgsensor.line_length * 1000),
		imgsensor.pclk, per_frame_ms);

	imgsensor.frame_length = (per_frame_ms + (ns / 1000000)) *
		imgsensor.frame_length /
		per_frame_ms;

	write_cmos_sensor_8(0x0104, 0x01);
	write_frame_len(imgsensor.frame_length);
	write_cmos_sensor_8(0x0104, 0x00);

	imgsensor.extend_frame_length_en = KAL_TRUE;

	LOG_INF("new frame len = %d, old frame len = %d, per_frame_ms = %d, add more %d ms",
			 imgsensor.frame_length, old_fl, per_frame_ms, (ns / 1000000));
}

static void sensor_init(void)
{
	LOG_INF("E\n");
	imx766dual_table_write_cmos_sensor(imx766dual_init_setting,
		sizeof(imx766dual_init_setting)/sizeof(kal_uint16));

	/*enable temperature sensor, TEMP_SEN_CTL:*/
	//write_cmos_sensor_8(0x0138, 0x01);

	set_mirror_flip(imgsensor.mirror);

	LOG_INF("X");
}	/*	  sensor_init  */

static void preview_setting(void)
{
	LOG_INF("E\n");

	imx766dual_table_write_cmos_sensor(imx766dual_preview_setting,
		sizeof(imx766dual_preview_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(0x32D2, 0x00);
	}

	LOG_INF("X");
} /* preview_setting */

/*full size 30fps*/
static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("%s(PD 012515) 30 fps E! currefps:%d\n", __func__, currefps);
	/*************MIPI output setting************/

	imx766dual_table_write_cmos_sensor(imx766dual_capture_30_setting,
		sizeof(imx766dual_capture_30_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(0x32D2, 0x00);
	}

	LOG_INF("%s(PD 012515) 30 fpsX\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("%s E! currefps:%d\n", __func__, currefps);
	imx766dual_table_write_cmos_sensor(imx766dual_normal_video_setting,
	sizeof(imx766dual_normal_video_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(0x32D2, 0x00);
	}

	LOG_INF("X\n");
}

static void hs_video_setting(void)
{
	preview_setting();

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(0x32D2, 0x00);
	}

}

static void hdr_write_tri_shutter_w_gph(kal_uint16 le, kal_uint16 me, kal_uint16 se, kal_bool gph)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 exposure_cnt = 0;

	if (le)
		exposure_cnt++;
	if (me)
		exposure_cnt++;
	if (se)
		exposure_cnt++;

	le = (kal_uint16)max(imgsensor_info.min_shutter, (kal_uint32)le);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = max((kal_uint32)(le + me + se + imgsensor_info.margin),
		imgsensor.min_frame_length);
	imgsensor.frame_length = min(imgsensor.frame_length, imgsensor_info.max_frame_length);
	spin_unlock(&imgsensor_drv_lock);
	//3exp 12 + 6, 2exp 8 + 6, 6 is fine integ time
	if (le && exposure_cnt) {
		le -= 6;   // subtract fine integ time 6
		le = round_up(le/exposure_cnt, 4);
	}

	if (me && exposure_cnt) {
		me -= 6;
		me = round_up(me/exposure_cnt, 4);
	}

	if (se && exposure_cnt) {
		se -= 6;
		se = round_up(se/exposure_cnt, 4);
	}

	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x autoflicker_en %d frame_length %d\n",
		le, me, se, imgsensor.autoflicker_en, imgsensor.frame_length);

	if (gph)
		write_cmos_sensor_8(0x0104, 0x01);

	if (imgsensor.autoflicker_en) {
		realtime_fps =
			imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	}
	// write_frame_len(imgsensor.frame_length);

	/* Long exposure */
	write_cmos_sensor_8(0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, le & 0xFF);
	/* Muddle exposure */
	if (me != 0) {
		/*MID_COARSE_INTEG_TIME[15:8]*/
		write_cmos_sensor_8(0x313A, (me >> 8) & 0xFF);
		/*MID_COARSE_INTEG_TIME[7:0]*/
		write_cmos_sensor_8(0x313B, me & 0xFF);
	}
	/* Short exposure */
	write_cmos_sensor_8(0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor_8(0x0225, se & 0xFF);
	if (gph)
		write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF("L! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);
}

static void hdr_write_tri_shutter(kal_uint16 le, kal_uint16 me, kal_uint16 se)
{
	hdr_write_tri_shutter_w_gph(le, me, se, KAL_TRUE);
}

static void hdr_write_tri_gain_w_gph(kal_uint16 lgain, kal_uint16 mg, kal_uint16 sg, kal_bool gph)
{
	kal_uint16 reg_lg, reg_mg, reg_sg;

	reg_lg = gain2reg(lgain);
	reg_mg = gain2reg(mg);
	reg_sg = gain2reg(sg);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_lg;
	spin_unlock(&imgsensor_drv_lock);
	if (gph)
		write_cmos_sensor_8(0x0104, 0x01);
	/* Long Gian */
	write_cmos_sensor_8(0x0204, (reg_lg>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_lg & 0xFF);
	/* Middle Gian */
	if (mg != 0) {
		write_cmos_sensor_8(0x313C, (reg_mg>>8) & 0xFF);
		write_cmos_sensor_8(0x313D, reg_mg & 0xFF);
	}
	/* Short Gian */
	write_cmos_sensor_8(0x0216, (reg_sg>>8) & 0xFF);
	write_cmos_sensor_8(0x0217, reg_sg & 0xFF);
	if (gph)
		write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF(
		"lgain:0x%x, reg_lg:0x%x, sg:0x%x, reg_mg:0x%x, mg:0x%x, reg_sg:0x%x\n",
		lgain, reg_lg, mg, reg_mg, sg, reg_sg);
}

static void hdr_write_tri_gain(kal_uint16 lgain, kal_uint16 mg, kal_uint16 sg)
{
	hdr_write_tri_gain_w_gph(lgain, mg, sg, KAL_TRUE);
}

#define PHASE_PIX_OUT_EN	0x30B4
#define FRAME_LEN_UPPER		0x0340
#define FRAME_LEN_LOWER		0x0341
#define CIT_LE_UPPER		0x0202
#define CIT_LE_LOWER		0x0203
#define CIT_ME_UPPER		0x0224
#define CIT_ME_LOWER		0x0225
#define CIT_SE_UPPER		0x313A
#define CIT_SE_LOWER		0x313B
#define DOL_EN				0x33D0
#define DOL_MODE			0x33D1
#define VSB_2ND_VCID		0x3070
#define VSB_3ND_VCID		0x3080

#define PIX_1_1ST_VCID		0x3066
#define PIX_2_1ST_VCID		0x3068
#define PIX_1_2ND_VCID		0x3077
#define PIX_2_2ND_VCID		0x3079
#define PIX_1_3RD_VCID		0x3087
#define PIX_2_3RD_VCID		0x3089

#define FMC_GPH_START		do { \
					write_cmos_sensor_8(0x0104, 0x01); \
					write_cmos_sensor_8(0x3010, 0x02); \
				} while (0)

#define FMC_GPH_END		do { \
					write_cmos_sensor_8(0x0104, 0x00); \
				} while (0)

enum {
	SHUTTER_NE_FRM_1 = 0,
	GAIN_NE_FRM_1,
	FRAME_LEN_NE_FRM_1,
	HDR_TYPE_FRM_1,
	SHUTTER_NE_FRM_2,
	GAIN_NE_FRM_2,
	FRAME_LEN_NE_FRM_2,
	HDR_TYPE_FRM_2,
	SHUTTER_SE_FRM_1,
	GAIN_SE_FRM_1,
	SHUTTER_SE_FRM_2,
	GAIN_SE_FRM_2,
	SHUTTER_ME_FRM_1,
	GAIN_ME_FRM_1,
	SHUTTER_ME_FRM_2,
	GAIN_ME_FRM_2,
};

static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id, uint32_t *ae_ctrl)
{
	imgsensor.extend_frame_length_en = KAL_FALSE;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	{
		kal_uint16 changed_reg_setting[] = {
			PHASE_PIX_OUT_EN, 0x01,
			FRAME_LEN_UPPER, 0x09,
			FRAME_LEN_LOWER, 0x74,
			DOL_EN, 0x00,
			DOL_MODE, 0x00
		};

		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_scenario_id = scenario_id;
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.custom2.pclk;
		imgsensor.line_length = imgsensor_info.custom2.linelength;
		imgsensor.frame_length = imgsensor_info.custom2.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
		spin_unlock(&imgsensor_drv_lock);

		FMC_GPH_START;
		imx766dual_table_write_cmos_sensor(changed_reg_setting,
				sizeof(changed_reg_setting) / sizeof(kal_uint16));

		if (ae_ctrl) {

			LOG_INF("call MSDK_SCENARIO_ID_CAMERA_PREVIEW %d %d",
				ae_ctrl[SHUTTER_NE_FRM_1],
				ae_ctrl[GAIN_NE_FRM_1]);
			set_shutter_w_gph(ae_ctrl[SHUTTER_NE_FRM_1], KAL_FALSE);
			set_gain_w_gph(ae_ctrl[GAIN_NE_FRM_1], KAL_FALSE);
		}
		FMC_GPH_END;
	}
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	{
		kal_uint16 changed_reg_setting[] = {
			PHASE_PIX_OUT_EN, 0x03,
			FRAME_LEN_UPPER, 0x09,
			FRAME_LEN_LOWER, 0x74,
			DOL_EN, 0x01,
			DOL_MODE, 0x00
		};

		spin_lock(&imgsensor_drv_lock);
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.custom2.pclk;
		imgsensor.line_length = imgsensor_info.custom2.linelength;
		imgsensor.frame_length = imgsensor_info.custom2.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
		spin_unlock(&imgsensor_drv_lock);

		FMC_GPH_START;
		imx766dual_table_write_cmos_sensor(changed_reg_setting,
				sizeof(changed_reg_setting) / sizeof(kal_uint16));

		if (ae_ctrl) {
			LOG_INF("call MSDK_SCENARIO_ID_CUSTOM2 %d %d %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1],
					0,
					ae_ctrl[SHUTTER_SE_FRM_1],
					ae_ctrl[GAIN_NE_FRM_1],
					0,
					ae_ctrl[GAIN_SE_FRM_1]);
			hdr_write_tri_shutter_w_gph(
					ae_ctrl[SHUTTER_NE_FRM_1],
					0,
					ae_ctrl[SHUTTER_SE_FRM_1],
					KAL_FALSE);
			hdr_write_tri_gain_w_gph(
					ae_ctrl[GAIN_NE_FRM_1],
					0,
					ae_ctrl[GAIN_SE_FRM_1],
					KAL_FALSE);
		}
		FMC_GPH_END;
	}
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
	{
		kal_uint16 changed_reg_setting[] = {
			PHASE_PIX_OUT_EN, 0x07,
			FRAME_LEN_UPPER, 0x06,
			FRAME_LEN_LOWER, 0x4C,
			DOL_EN, 0x01,
			DOL_MODE, 0x01
		};

		spin_lock(&imgsensor_drv_lock);
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.custom3.pclk;
		imgsensor.line_length = imgsensor_info.custom3.linelength;
		imgsensor.frame_length = imgsensor_info.custom3.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
		spin_unlock(&imgsensor_drv_lock);

		FMC_GPH_START;
		imx766dual_table_write_cmos_sensor(changed_reg_setting,
				sizeof(changed_reg_setting) / sizeof(kal_uint16));

		if (ae_ctrl) {
			LOG_INF("call MSDK_SCENARIO_ID_CUSTOM3 %d %d %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1],
					ae_ctrl[SHUTTER_ME_FRM_1],
					ae_ctrl[SHUTTER_SE_FRM_1],
					ae_ctrl[GAIN_NE_FRM_1],
					ae_ctrl[GAIN_ME_FRM_1],
					ae_ctrl[GAIN_SE_FRM_1]);
			hdr_write_tri_shutter_w_gph(
					ae_ctrl[SHUTTER_NE_FRM_1],
					ae_ctrl[SHUTTER_ME_FRM_1],
					ae_ctrl[SHUTTER_SE_FRM_1],
					KAL_FALSE);
			hdr_write_tri_gain_w_gph(
					ae_ctrl[GAIN_NE_FRM_1],
					ae_ctrl[GAIN_ME_FRM_1],
					ae_ctrl[GAIN_SE_FRM_1],
					KAL_FALSE);
		}
		FMC_GPH_END;
	}
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	{
		kal_uint16 changed_reg_setting[] = {
			PHASE_PIX_OUT_EN, 0x01,
			FRAME_LEN_UPPER, 0x12,
			FRAME_LEN_LOWER, 0xEC,
			DOL_EN, 0x00,
			DOL_MODE, 0x00
		};

		spin_lock(&imgsensor_drv_lock);
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.normal_video.pclk;
		imgsensor.line_length = imgsensor_info.normal_video.linelength;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength;
		imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
		spin_unlock(&imgsensor_drv_lock);

		FMC_GPH_START;
		imx766dual_table_write_cmos_sensor(changed_reg_setting,
				sizeof(changed_reg_setting) / sizeof(kal_uint16));

		if (ae_ctrl) {
			LOG_INF("call MSDK_SCENARIO_ID_VIDEO_PREVIEW %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1]);
			set_shutter_w_gph(ae_ctrl[SHUTTER_NE_FRM_1], KAL_FALSE);
			set_gain_w_gph(ae_ctrl[GAIN_NE_FRM_1], KAL_FALSE);
		}
		FMC_GPH_END;
	}
		break;
	default:
	{
		LOG_INF("%s error! wrong setting in set_seamless_switch = %d",
			__func__, scenario_id);

		return 0xff;
	}
	}

	imgsensor.fast_mode_on = KAL_TRUE;
	LOG_INF("%s success, scenario is switched to %d", __func__, scenario_id);
	return 0;
}

static void slim_video_setting(void)
{
	LOG_INF("%s E\n", __func__);
	hs_video_setting();

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(0x32D2, 0x00);
	}
}

/*full size 16M@24fps*/
static void custom1_setting(void)
{
	LOG_INF("%s 240 fps E! currefps\n", __func__);
	/*************MIPI output setting************/

	imx766dual_table_write_cmos_sensor(imx766dual_custom1_setting,
		sizeof(imx766dual_custom1_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(0x32D2, 0x00);
	}

	LOG_INF("X");
}

/*full size 8M@24fps*/
static void custom2_setting(void)
{
	LOG_INF("%s 480 fps E! currefps\n", __func__);
	/*************MIPI output setting************/

	imx766dual_table_write_cmos_sensor(imx766dual_custom2_setting,
		sizeof(imx766dual_custom2_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(0x32D2, 0x00);
	}

	LOG_INF("X");
}

/*full size 16M@24fps*/
static void custom3_setting(void)
{
	LOG_INF("%s 4M*60 fps E! currefps\n", __func__);
	/*************MIPI output setting************/

	imx766dual_table_write_cmos_sensor(imx766dual_custom3_setting,
		sizeof(imx766dual_custom3_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(0x32D2, 0x00);
	}

	LOG_INF("X");
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
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
					| read_cmos_sensor_8(0x0017));
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_info("[%s] i2c write id: 0x%x, sensor id: 0x%x\n",
					__func__, imgsensor.i2c_write_id, *sensor_id);
				read_sensor_Cali();
				return ERROR_NONE;
			}

			LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
			LOG_INF("sensor_id = 0x%x, imgsensor_info.sensor_id = 0x%x\n",
				*sensor_id, imgsensor_info.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct,
		 *Must set *sensor_id to 0xFFFFFFFF
		 */
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

	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
					| read_cmos_sensor_8(0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
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
} /* open */

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

	write_cmos_sensor_8(0x0100, 0x00);
	qsc_flag = 0;
	return ERROR_NONE;
} /* close */


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
	LOG_INF("%s E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	preview_setting();

	//if (imgsensor.pdaf_mode)
	//	imx766dual_apply_LRC();

	return ERROR_NONE;
} /* preview */

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

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	capture_setting(imgsensor.current_fps);

	/* set_mirror_flip(imgsensor.mirror); */

	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	normal_video_setting(imgsensor.current_fps);


	/*set_mirror_flip(imgsensor.mirror);*/

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	hs_video_setting();
	/*set_mirror_flip(imgsensor.mirror);*/

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	slim_video_setting();
	/*set_mirror_flip(imgsensor.mirror);*/

	return ERROR_NONE;
}	/* slim_video */


static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	custom1_setting();

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	custom2_setting();

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	custom3_setting();

	return ERROR_NONE;
}	/* custom3 */

static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
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

	return ERROR_NONE;
} /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
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
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
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
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV_QPD;

	sensor_info->HDR_Support = HDR_SUPPORT_STAGGER_FDOL;

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
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
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
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
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
	write_cmos_sensor_8(0x0104, 0x01);
	set_max_framerate(imgsensor.current_fps, 1);
	write_cmos_sensor_8(0x0104, 0x00);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker*/
		imgsensor.autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
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
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
				"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
				, framerate
				, imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk / framerate * 10
					/ imgsensor_info.cap.linelength;

		if (frame_length > imgsensor_info.max_frame_length) {
			LOG_INF(
				"Warning: frame_length %d > max_frame_length %d!\n"
				, frame_length
				, imgsensor_info.max_frame_length);
			break;
		}

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.cap.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
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
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
				/ imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom1.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
				/ imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom2.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
				/ imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
			? (frame_length - imgsensor_info.custom3.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom3.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			write_cmos_sensor_8(0x0104, 0x01);
			set_dummy();
			write_cmos_sensor_8(0x0104, 0x00);
		}
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{

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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor_8(0x0601, 0x0002); /*100% Color bar*/
	else
		write_cmos_sensor_8(0x0601, 0x0000); /*No pattern*/

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 imx766dual_ana_gain_table[] = {
	100000,
	100098,
	100196,
	100294,
	100392,
	100491,
	100589,
	100688,
	100787,
	100887,
	100986,
	101086,
	101186,
	101286,
	101386,
	101487,
	101587,
	101688,
	101789,
	101891,
	101992,
	102094,
	102196,
	102298,
	102400,
	102503,
	102605,
	102708,
	102811,
	102915,
	103018,
	103122,
	103226,
	103330,
	103434,
	103539,
	103644,
	103749,
	103854,
	103959,
	104065,
	104171,
	104277,
	104383,
	104490,
	104597,
	104703,
	104811,
	104918,
	105026,
	105133,
	105242,
	105350,
	105458,
	105567,
	105676,
	105785,
	105895,
	106004,
	106114,
	106224,
	106334,
	106445,
	106556,
	106667,
	106778,
	106889,
	107001,
	107113,
	107225,
	107338,
	107450,
	107563,
	107676,
	107789,
	107903,
	108017,
	108131,
	108245,
	108360,
	108475,
	108590,
	108705,
	108820,
	108936,
	109052,
	109168,
	109285,
	109402,
	109519,
	109636,
	109753,
	109871,
	109989,
	110108,
	110226,
	110345,
	110464,
	110583,
	110703,
	110823,
	110943,
	111063,
	111183,
	111304,
	111425,
	111547,
	111668,
	111790,
	111913,
	112035,
	112158,
	112281,
	112404,
	112527,
	112651,
	112775,
	112900,
	113024,
	113149,
	113274,
	113400,
	113525,
	113651,
	113778,
	113904,
	114031,
	114158,
	114286,
	114413,
	114541,
	114670,
	114798,
	114927,
	115056,
	115186,
	115315,
	115445,
	115576,
	115706,
	115837,
	115968,
	116100,
	116232,
	116364,
	116496,
	116629,
	116762,
	116895,
	117029,
	117162,
	117297,
	117431,
	117566,
	117701,
	117837,
	117972,
	118108,
	118245,
	118382,
	118519,
	118656,
	118794,
	118931,
	119070,
	119208,
	119347,
	119487,
	119626,
	119766,
	119906,
	120047,
	120188,
	120329,
	120471,
	120612,
	120755,
	120897,
	121040,
	121183,
	121327,
	121471,
	121615,
	121760,
	121905,
	122050,
	122196,
	122342,
	122488,
	122635,
	122782,
	122929,
	123077,
	123225,
	123373,
	123522,
	123671,
	123821,
	123971,
	124121,
	124272,
	124423,
	124574,
	124726,
	124878,
	125031,
	125183,
	125337,
	125490,
	125644,
	125799,
	125953,
	126108,
	126264,
	126420,
	126576,
	126733,
	126890,
	127047,
	127205,
	127363,
	127522,
	127681,
	127840,
	128000,
	128160,
	128321,
	128482,
	128643,
	128805,
	128967,
	129130,
	129293,
	129456,
	129620,
	129785,
	129949,
	130114,
	130280,
	130446,
	130612,
	130779,
	130946,
	131114,
	131282,
	131451,
	131620,
	131789,
	131959,
	132129,
	132300,
	132471,
	132642,
	132815,
	132987,
	133160,
	133333,
	133507,
	133681,
	133856,
	134031,
	134207,
	134383,
	134560,
	134737,
	134914,
	135092,
	135271,
	135450,
	135629,
	135809,
	135989,
	136170,
	136352,
	136533,
	136716,
	136898,
	137082,
	137265,
	137450,
	137634,
	137820,
	138005,
	138192,
	138378,
	138566,
	138753,
	138942,
	139130,
	139320,
	139510,
	139700,
	139891,
	140082,
	140274,
	140466,
	140659,
	140853,
	141047,
	141241,
	141436,
	141632,
	141828,
	142025,
	142222,
	142420,
	142618,
	142817,
	143017,
	143217,
	143417,
	143619,
	143820,
	144023,
	144225,
	144429,
	144633,
	144837,
	145042,
	145248,
	145455,
	145661,
	145869,
	146077,
	146286,
	146495,
	146705,
	146915,
	147126,
	147338,
	147550,
	147763,
	147977,
	148191,
	148406,
	148621,
	148837,
	149054,
	149271,
	149489,
	149708,
	149927,
	150147,
	150367,
	150588,
	150810,
	151032,
	151256,
	151479,
	151704,
	151929,
	152155,
	152381,
	152608,
	152836,
	153064,
	153293,
	153523,
	153754,
	153985,
	154217,
	154449,
	154683,
	154917,
	155152,
	155387,
	155623,
	155860,
	156098,
	156336,
	156575,
	156815,
	157055,
	157296,
	157538,
	157781,
	158025,
	158269,
	158514,
	158760,
	159006,
	159253,
	159502,
	159750,
	160000,
	160250,
	160502,
	160754,
	161006,
	161260,
	161514,
	161769,
	162025,
	162282,
	162540,
	162798,
	163057,
	163317,
	163578,
	163840,
	164103,
	164366,
	164630,
	164895,
	165161,
	165428,
	165696,
	165964,
	166234,
	166504,
	166775,
	167047,
	167320,
	167594,
	167869,
	168144,
	168421,
	168699,
	168977,
	169256,
	169536,
	169818,
	170100,
	170383,
	170667,
	170952,
	171237,
	171524,
	171812,
	172101,
	172391,
	172681,
	172973,
	173266,
	173559,
	173854,
	174150,
	174446,
	174744,
	175043,
	175342,
	175643,
	175945,
	176248,
	176552,
	176857,
	177163,
	177470,
	177778,
	178087,
	178397,
	178709,
	179021,
	179335,
	179649,
	179965,
	180282,
	180600,
	180919,
	181239,
	181560,
	181883,
	182206,
	182531,
	182857,
	183184,
	183513,
	183842,
	184173,
	184505,
	184838,
	185172,
	185507,
	185844,
	186182,
	186521,
	186861,
	187203,
	187546,
	187890,
	188235,
	188582,
	188930,
	189279,
	189630,
	189981,
	190335,
	190689,
	191045,
	191402,
	191760,
	192120,
	192481,
	192844,
	193208,
	193573,
	193939,
	194307,
	194677,
	195048,
	195420,
	195793,
	196169,
	196545,
	196923,
	197303,
	197683,
	198066,
	198450,
	198835,
	199222,
	199610,
	200000,
	200391,
	200784,
	201179,
	201575,
	201972,
	202372,
	202772,
	203175,
	203579,
	203984,
	204391,
	204800,
	205210,
	205622,
	206036,
	206452,
	206869,
	207287,
	207708,
	208130,
	208554,
	208980,
	209407,
	209836,
	210267,
	210700,
	211134,
	211570,
	212008,
	212448,
	212890,
	213333,
	213779,
	214226,
	214675,
	215126,
	215579,
	216034,
	216490,
	216949,
	217410,
	217872,
	218337,
	218803,
	219272,
	219742,
	220215,
	220690,
	221166,
	221645,
	222126,
	222609,
	223094,
	223581,
	224070,
	224561,
	225055,
	225551,
	226049,
	226549,
	227051,
	227556,
	228062,
	228571,
	229083,
	229596,
	230112,
	230631,
	231151,
	231674,
	232200,
	232727,
	233257,
	233790,
	234325,
	234862,
	235402,
	235945,
	236490,
	237037,
	237587,
	238140,
	238695,
	239252,
	239813,
	240376,
	240941,
	241509,
	242080,
	242654,
	243230,
	243810,
	244391,
	244976,
	245564,
	246154,
	246747,
	247343,
	247942,
	248544,
	249148,
	249756,
	250367,
	250980,
	251597,
	252217,
	252840,
	253465,
	254094,
	254726,
	255362,
	256000,
	256642,
	257286,
	257935,
	258586,
	259241,
	259898,
	260560,
	261224,
	261893,
	262564,
	263239,
	263918,
	264599,
	265285,
	265974,
	266667,
	267363,
	268063,
	268766,
	269474,
	270185,
	270899,
	271618,
	272340,
	273067,
	273797,
	274531,
	275269,
	276011,
	276757,
	277507,
	278261,
	279019,
	279781,
	280548,
	281319,
	282094,
	282873,
	283657,
	284444,
	285237,
	286034,
	286835,
	287640,
	288451,
	289266,
	290085,
	290909,
	291738,
	292571,
	293410,
	294253,
	295101,
	295954,
	296812,
	297674,
	298542,
	299415,
	300293,
	301176,
	302065,
	302959,
	303858,
	304762,
	305672,
	306587,
	307508,
	308434,
	309366,
	310303,
	311246,
	312195,
	313150,
	314110,
	315077,
	316049,
	317028,
	318012,
	319003,
	320000,
	321003,
	322013,
	323028,
	324051,
	325079,
	326115,
	327157,
	328205,
	329260,
	330323,
	331392,
	332468,
	333550,
	334641,
	335738,
	336842,
	337954,
	339073,
	340199,
	341333,
	342475,
	343624,
	344781,
	345946,
	347119,
	348299,
	349488,
	350685,
	351890,
	353103,
	354325,
	355556,
	356794,
	358042,
	359298,
	360563,
	361837,
	363121,
	364413,
	365714,
	367025,
	368345,
	369675,
	371014,
	372364,
	373723,
	375092,
	376471,
	377860,
	379259,
	380669,
	382090,
	383521,
	384962,
	386415,
	387879,
	389354,
	390840,
	392337,
	393846,
	395367,
	396899,
	398444,
	400000,
	401569,
	403150,
	404743,
	406349,
	407968,
	409600,
	411245,
	412903,
	414575,
	416260,
	417959,
	419672,
	421399,
	423140,
	424896,
	426667,
	428452,
	430252,
	432068,
	433898,
	435745,
	437607,
	439485,
	441379,
	443290,
	445217,
	447162,
	449123,
	451101,
	453097,
	455111,
	457143,
	459193,
	461261,
	463348,
	465455,
	467580,
	469725,
	471889,
	474074,
	476279,
	478505,
	480751,
	483019,
	485308,
	487619,
	489952,
	492308,
	494686,
	497087,
	499512,
	501961,
	504433,
	506931,
	509453,
	512000,
	514573,
	517172,
	519797,
	522449,
	525128,
	527835,
	530570,
	533333,
	536126,
	538947,
	541799,
	544681,
	547594,
	550538,
	553514,
	556522,
	559563,
	562637,
	565746,
	568889,
	572067,
	575281,
	578531,
	581818,
	585143,
	588506,
	591908,
	595349,
	598830,
	602353,
	605917,
	609524,
	613174,
	616867,
	620606,
	624390,
	628221,
	632099,
	636025,
	640000,
	644025,
	648101,
	652229,
	656410,
	660645,
	664935,
	669281,
	673684,
	678146,
	682667,
	687248,
	691892,
	696599,
	701370,
	706207,
	711111,
	716084,
	721127,
	726241,
	731429,
	736691,
	742029,
	747445,
	752941,
	758519,
	764179,
	769925,
	775758,
	781679,
	787692,
	793798,
	800000,
	806299,
	812698,
	819200,
	825806,
	832520,
	839344,
	846281,
	853333,
	860504,
	867797,
	875214,
	882759,
	890435,
	898246,
	906195,
	914286,
	922523,
	930909,
	939450,
	948148,
	957009,
	966038,
	975238,
	984615,
	994175,
	1003922,
	1013861,
	1024000,
	1034343,
	1044898,
	1055670,
	1066667,
	1077895,
	1089362,
	1101075,
	1113043,
	1125275,
	1137778,
	1150562,
	1163636,
	1177011,
	1190698,
	1204706,
	1219048,
	1233735,
	1248780,
	1264198,
	1280000,
	1296203,
	1312821,
	1329870,
	1347368,
	1365333,
	1383784,
	1402740,
	1422222,
	1442254,
	1462857,
	1484058,
	1505882,
	1528358,
	1551515,
	1575385,
	1600000,
};

static kal_int32 get_sensor_temperature(void)
{
	UINT8 temperature = 0;
	INT32 temperature_convert = 0;

	temperature = read_cmos_sensor_8(0x013a);

	if (temperature <= 0x60)
		temperature_convert = temperature;
	else if (temperature >= 0x61 && temperature <= 0x7F)
		temperature_convert = 97;
	else if (temperature >= 0x80 && temperature <= 0xE2)
		temperature_convert = -30;
	else
		temperature_convert = (INT8)temperature | 0xFFFFFF0;

	return temperature_convert;
}
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	struct SENSOR_VC_INFO2_STRUCT *pvcinfo2;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	switch (feature_id) {
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(imx766dual_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)imx766dual_ana_gain_table,
			sizeof(imx766dual_ana_gain_table));
		}
		break;
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
		*(feature_data + 2) = 24; // 2/3 exp do not have to round up
		break;
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 3000000;
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		{
			MUINT32 ratio = 1;

			if (*(feature_data + 2) & SENSOR_GET_LINELENGTH_FOR_READOUT)
				ratio = get_exp_cnt_by_scenario((*feature_data));

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.cap.framelength << 16)
					+ (ratio * imgsensor_info.cap.linelength);
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.normal_video.framelength << 16)
					+ (ratio * imgsensor_info.normal_video.linelength);
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.hs_video.framelength << 16)
					+ (ratio * imgsensor_info.hs_video.linelength);
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.slim_video.framelength << 16)
					+ (ratio * imgsensor_info.slim_video.linelength);
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom1.framelength << 16)
					+ (ratio * imgsensor_info.custom1.linelength);
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom2.framelength << 16)
					+ (ratio * imgsensor_info.custom2.linelength);
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom3.framelength << 16)
					+ (ratio * imgsensor_info.custom3.linelength);
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre.framelength << 16)
					+ (ratio * imgsensor_info.pre.linelength);
				break;
			}
		}
		break;
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
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
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
	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
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
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:

		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting */
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH:
		pr_debug("extend_frame_len %d\n", *feature_data);
		extend_frame_length((MUINT32) *feature_data);
		pr_debug("extend_frame_len done %d\n", *feature_data);
		break;
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
	{
		LOG_INF("SENSOR_FEATURE_SEAMLESS_SWITCH");
		if ((feature_data + 1) != NULL)
			pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		else
			pr_debug("warning! no ae_ctrl input");

		if (feature_data == NULL) {
			LOG_INF("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}
		LOG_INF("call seamless_switch");
		seamless_switch((*feature_data), pAeCtrls);
	}
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		if ((feature_data + 1) != NULL)
			pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		else {
			LOG_INF("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*pScenarios = MSDK_SCENARIO_ID_CUSTOM2;
			*(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM3;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
			*(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM3;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
			*(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM2;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		default:
			*pScenarios = 0xff;
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
			*feature_data, *pScenarios);
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0xB;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0xC;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		}
		LOG_INF(
			"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n",
			*feature_data, *(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
		/*END OF HDR CMD */
		case SENSOR_FEATURE_GET_VC_INFO2: {
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO2 %d\n",
							(UINT16) (*feature_data));
		pvcinfo2 = (struct SENSOR_VC_INFO2_STRUCT *) (uintptr_t) (*(feature_data + 1));
		get_vc_info_2(pvcinfo2, *feature_data_32);
		}
		break;
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
		if (*feature_data == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM2;
				break;
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM3;
				break;
			default:
				break;
			}
		} else if (*feature_data == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM2;
				break;
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM3;
				break;
			default:
				break;
			}
		} else if (*feature_data == MSDK_SCENARIO_ID_CUSTOM2) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
				break;
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM3;
				break;
			default:
				break;
			}
		} else if (*feature_data == MSDK_SCENARIO_ID_CUSTOM3) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
				break;
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM2;
				break;
			default:
				break;
			}
		}
		LOG_INF("SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO %d %d %d\n",
				(UINT16) (*feature_data),
				(UINT16) *(feature_data + 1),
				(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		*(feature_data + 1) = 1; //always 1
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		if (*feature_data == MSDK_SCENARIO_ID_CUSTOM2
			|| *feature_data == MSDK_SCENARIO_ID_CUSTOM3) {
			// see IMX766DUAL SRM, table 5-22 constraints of COARSE_INTEG_TIME
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
			case VC_STAGGER_ME:
			case VC_STAGGER_SE:
			default:
				*(feature_data + 2) = 65532 - imgsensor_info.margin;
				break;
			}
		} else
			*(feature_data + 2) = 0;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER://for 2EXP
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
				(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		// implement write shutter for NE/SE
		hdr_write_tri_shutter((UINT16)*feature_data,
					0,
					(UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN://for 2EXP
		LOG_INF("SENSOR_FEATURE_SET_DUAL_GAIN LE=%d, SE=%d\n",
				(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		// implement write gain for NE/SE
		hdr_write_tri_gain((UINT16)*feature_data,
					0,
					(UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER://for 3EXP
		LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_SHUTTER LE=%d, ME=%d, SE=%d\n",
					(UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		// implement write shutter for NE/ME/SE
		hdr_write_tri_shutter((UINT16)*feature_data,
					(UINT16)*(feature_data+1),
					(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN:
		LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_GAIN LGain=%d, SGain=%d, MGain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		// implement write gain for NE/ME/SE
		hdr_write_tri_gain((UINT16)*feature_data,
					(UINT16)*(feature_data+1),
					(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		imx766dual_get_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		/*LOG_INF("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
		 *	(*feature_para_len));
		 */
		imx766dual_set_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_INF("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(
			(UINT16) (*feature_data),
			(UINT16) (*(feature_data + 1)),
			(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
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
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
			*feature_return_para_32 = 1465;
			break;
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		memcpy(feature_return_para_32,
		&imgsensor.ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
		break;
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		*feature_return_para_32 = imgsensor.current_ae_effective_frame;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
	break;
	default:
		break;
	}

	return ERROR_NONE;
} /* feature_control() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX766DUAL_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
} /* IMX766DUAL_MIPI_RAW_SensorInit */
