// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

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
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "imx766dualmipiraw_Sensor.h"
#include "imx766dual_eeprom.h"
#include "imx766dual_ana_gain_table.h"
#include "imx766dual_Sensor_setting.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor_16(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor_16(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define imx766dual_table_write_cmos_sensor_8(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)

#define IMX766DUAL_EEPROM_READ_ID  0xA2
#define IMX766DUAL_EEPROM_WRITE_ID 0xA3

/***************Modify Following Strings for Debug**********************/
#define PFX "IMX766DUAL_camera_sensor"
/****************************   Modify end	**************************/
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

static kal_uint8 qsc_flag;
static kal_uint8 otp_flag;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX766DUAL_SENSOR_ID,

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
	.min_gain = BASEGAIN * 1, /*1x gain*/
	.max_gain = BASEGAIN * 64, /*64x gain*/
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
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[1],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[2],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[3],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[4],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[5],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	default:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	}
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

static kal_uint32 get_cur_exp_cnt(struct subdrv_ctx *ctx)
{
	kal_uint32 exp_cnt = 1;

	if (0x1 == (read_cmos_sensor_8(ctx, 0x33D0) & 0x1)) { // DOL_EN
		if (0x1 == (read_cmos_sensor_8(ctx, 0x33D1) & 0x3)) { // DOL_MODE
			exp_cnt = 3;
		} else {
			exp_cnt = 2;
		}
	}

	return exp_cnt;
}

static void imx766dual_get_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(ctx, regDa[idx]);
	}
}
static void imx766dual_set_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	imx766dual_table_write_cmos_sensor_8(ctx, regDa, regNum*2);
}

static kal_uint16 read_cmos_eeprom_8(struct subdrv_ctx *ctx, kal_uint16 addr)
{
	kal_uint16 get_byte = 0;

	adaptor_i2c_rd_u8(ctx->i2c_client, IMX766DUAL_EEPROM_READ_ID >> 1, addr, (u8 *)&get_byte);
	return get_byte;
}

#define QSC_SIZE 3072
#define QSC_EEPROM_ADDR 0x22C0
#define QSC_OTP_ADDR 0xC800
#define SENSOR_ID_L 0x05
#define SENSOR_ID_H 0x00
#define LENS_ID_L 0x47
#define LENS_ID_H 0x01
static void read_sensor_Cali(struct subdrv_ctx *ctx)
{
	kal_uint16 idx = 0, addr_qsc = QSC_EEPROM_ADDR, sensor_qsc = QSC_OTP_ADDR;
	kal_uint8 otp_data[9] = {0};
	int i = 0;

	/*read otp data to distinguish module*/
	otp_flag = OTP_QSC_NONE;

	for (i = 0; i < 7; i++)
		otp_data[i] = read_cmos_eeprom_8(ctx, 0x0006 + i);

	/*Internal Module Type*/
	if ((otp_data[0] == SENSOR_ID_L) &&
		(otp_data[1] == SENSOR_ID_H) &&
		(otp_data[2] == LENS_ID_L) &&
		(otp_data[3] == LENS_ID_H)) {
		LOG_INF("OTP type: Custom Only");
		otp_flag = OTP_QSC_CUSTOM;

		for (idx = 0; idx < QSC_SIZE; idx++) {
			addr_qsc = QSC_EEPROM_ADDR + idx;
			sensor_qsc = QSC_OTP_ADDR + idx;
			imx766dual_QSC_setting[2 * idx] = sensor_qsc;
			imx766dual_QSC_setting[2 * idx + 1] =
				read_cmos_eeprom_8(ctx, addr_qsc);
		}
	} else {
		LOG_INF("OTP type: No Data, 0x0008 = %d, 0x0009 = %d",
		read_cmos_eeprom_8(ctx, 0x0008), read_cmos_eeprom_8(ctx, 0x0009));
	}
}

static void write_sensor_QSC(struct subdrv_ctx *ctx)
{
	// calibration tool version 3.0 -> 0x4E
	write_cmos_sensor_8(ctx, 0x86A9, 0x4E);
	// set QSC from EEPROM to sensor
	if ((otp_flag == OTP_QSC_CUSTOM) || (otp_flag == OTP_QSC_INTERNAL)) {
		imx766dual_table_write_cmos_sensor_8(ctx, imx766dual_QSC_setting,
		sizeof(imx766dual_QSC_setting) / sizeof(kal_uint16));
	}
	write_cmos_sensor_8(ctx, 0x32D2, 0x01);
}

static void write_frame_len(struct subdrv_ctx *ctx, kal_uint32 fll)
{
	// //write_frame_len should be called inside GRP_PARAM_HOLD (0x0104)
	// FRM_LENGTH_LINES must be multiple of 4
	kal_uint32 exp_cnt = get_cur_exp_cnt(ctx);

	ctx->frame_length = round_up(fll / exp_cnt, 4) * exp_cnt;

	if (ctx->extend_frame_length_en == KAL_FALSE) {
		LOG_INF("fll %d exp_cnt %d\n", ctx->frame_length, exp_cnt);
		write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length / exp_cnt >> 8);
		write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length / exp_cnt & 0xFF);
	}

	if (ctx->fast_mode_on == KAL_TRUE) {
		ctx->fast_mode_on = KAL_FALSE;
		write_cmos_sensor_8(ctx, 0x3010, 0x00);
	}
}

static void set_dummy(struct subdrv_ctx *ctx)
{

	LOG_INF("dummyline = %d, dummypixels = %d\n",
		ctx->dummy_line, ctx->dummy_pixel);

	/* return;*/ /* for test */
	write_frame_len(ctx, ctx->frame_length);

}	/*	set_dummy  */

static void set_mirror_flip(struct subdrv_ctx *ctx, kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(ctx, 0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
	write_cmos_sensor_8(ctx, 0x0101, itemp);
	break;

	case IMAGE_V_MIRROR:
	write_cmos_sensor_8(ctx, 0x0101, itemp | 0x02);
	break;

	case IMAGE_H_MIRROR:
	write_cmos_sensor_8(ctx, 0x0101, itemp | 0x01);
	break;

	case IMAGE_HV_MIRROR:
	write_cmos_sensor_8(ctx, 0x0101, itemp | 0x03);
	break;
	}
}

static void set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = ctx->frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n", framerate,
		min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
	if (frame_length >= ctx->min_frame_length)
		ctx->frame_length = frame_length;
	else
		ctx->frame_length = ctx->min_frame_length;

	ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;
		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	set_dummy(ctx);
}	/*	set_max_framerate  */

#define MAX_CIT_LSHIFT 7
static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter, kal_bool gph)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 l_shift = 1;

	if (ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM1)
		shutter -= 2;

	shutter = round_up(shutter, 4);

	if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10
				/ ctx->frame_length;
		LOG_INF("autoflicker enable, realtime_fps = %d\n",
			realtime_fps);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
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
		ctx->frame_length = shutter + imgsensor_info.margin;
		LOG_INF("enter long exposure mode, time is %d", l_shift);
		write_cmos_sensor_8(ctx, 0x3100,
			read_cmos_sensor_16(ctx, 0x3100) | (l_shift & 0x7));
		/* Frame exposure mode customization for LE*/
		ctx->ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		ctx->ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		ctx->current_ae_effective_frame = 2;
	} else {
		write_cmos_sensor_8(ctx, 0x3100, read_cmos_sensor_16(ctx, 0x3100) & 0xf8);
		write_frame_len(ctx, ctx->frame_length);
		ctx->current_ae_effective_frame = 2;
		LOG_INF("set frame_length\n");
	}

	/* Update Shutter */
	write_cmos_sensor_8(ctx, 0x0350, 0x01); /* Enable auto extend */
	write_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, shutter  & 0xFF);
	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x00);

	LOG_INF("shutter =%d, framelength =%d\n",
		shutter, ctx->frame_length);
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
static void set_shutter_w_gph(struct subdrv_ctx *ctx, kal_uint32 shutter, kal_bool gph)
{
	ctx->shutter = shutter;

	write_shutter(ctx, shutter, gph);
}
static void set_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	set_shutter_w_gph(ctx, shutter, KAL_TRUE);
} /* set_shutter */

static void set_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint16 shutter, kal_uint16 frame_length,
				kal_bool auto_extend_en)
{	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	ctx->shutter = shutter;

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
	/*Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;
	ctx->frame_length = ctx->frame_length + dummy_line;

	/*  */
	if (shutter > ctx->frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;

	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk
			/ ctx->line_length * 10 / ctx->frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			/* Extend frame length */
			write_frame_len(ctx, ctx->frame_length);
		}
	} else {
		/* Extend frame length */
		write_frame_len(ctx, ctx->frame_length);
	}

	/* Update Shutter */
	if (auto_extend_en)
		write_cmos_sensor_8(ctx, 0x0350, 0x01); /* Enable auto extend */
	else
		write_cmos_sensor_8(ctx, 0x0350, 0x00); /* Disable auto extend */
	write_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, shutter & 0xFF);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	LOG_INF(
	    "Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
	    shutter, ctx->frame_length,
	    frame_length, dummy_line,
	    read_cmos_sensor_8(ctx, 0x0350));
}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;
	kal_uint16 gain_value = gain;
#ifdef USE_GAIN_TABLE
	int i = 0;
#endif

	if (gain_value < imgsensor_info.min_gain || gain_value > imgsensor_info.max_gain) {
		LOG_INF("Error: gain value out of range");

		if (gain_value < imgsensor_info.min_gain)
			gain_value = imgsensor_info.min_gain;
		else if (gain_value > imgsensor_info.max_gain)
			gain_value = imgsensor_info.max_gain;
	}

#ifdef USE_GAIN_TABLE
	reg_gain = imx766dual_gain_reg[IMX766DUAL_GAIN_TABLE_SIZE - 1];
	for (i = 0; i < IMX766DUAL_GAIN_TABLE_SIZE; i++) {
		if (gain_value <= imx766dual_gain_ratio[i]) {
			reg_gain = imx766dual_gain_reg[i];
			break;
		}
	}
#else
	reg_gain = 16384 - (16384 * BASEGAIN) / gain_value;
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
 *	iGain : sensor global gain(base: 0x400)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain_w_gph(struct subdrv_ctx *ctx, kal_uint16 gain, kal_bool gph)
{
	kal_uint16 reg_gain;

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	LOG_INF("gain = %d, reg_gain = 0x%x\n ", gain, reg_gain);

	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_cmos_sensor_8(ctx, 0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0205, reg_gain & 0xFF);
	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x00);

	return gain;
}

static kal_uint16 set_gain(struct subdrv_ctx *ctx, kal_uint16 gain)
{
	return set_gain_w_gph(ctx, gain, KAL_TRUE);
} /* set_gain */

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable)
		write_cmos_sensor_8(ctx, 0x0100, 0X01);
	else
		write_cmos_sensor_8(ctx, 0x0100, 0x00);
	return ERROR_NONE;
}

static void extend_frame_length(struct subdrv_ctx *ctx, kal_uint32 ns)
{
	UINT32 old_fl = ctx->frame_length;

	kal_uint32 per_frame_ms = (kal_uint32)(((unsigned long long)ctx->frame_length *
		(unsigned long long)ctx->line_length * 1000) / (unsigned long long)ctx->pclk);

	LOG_INF("per_frame_ms: %d / %d = %d",
		(ctx->frame_length * ctx->line_length * 1000), ctx->pclk, per_frame_ms);

	ctx->frame_length = (per_frame_ms + (ns / 1000000)) * ctx->frame_length / per_frame_ms;

	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_frame_len(ctx, ctx->frame_length);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	ctx->extend_frame_length_en = KAL_TRUE;

	LOG_INF("new frame len = %d, old frame len = %d, per_frame_ms = %d, add more %d ms",
		ctx->frame_length, old_fl, per_frame_ms, (ns / 1000000));
}

static void sensor_init(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");
	imx766dual_table_write_cmos_sensor_8(ctx, imx766dual_init_setting,
		sizeof(imx766dual_init_setting)/sizeof(kal_uint16));

	set_mirror_flip(ctx, ctx->mirror);

	LOG_INF("X");
}	/*	  sensor_init  */

static void preview_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");

	imx766dual_table_write_cmos_sensor_8(ctx, imx766dual_preview_setting,
		sizeof(imx766dual_preview_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x32D2, 0x00);
	}

	LOG_INF("X");
} /* preview_setting */

/*full size 30fps*/
static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	LOG_INF("%s(PD 012515) 30 fps E! currefps:%d\n", __func__, currefps);
	/*************MIPI output setting************/

	imx766dual_table_write_cmos_sensor_8(ctx, imx766dual_capture_30_setting,
		sizeof(imx766dual_capture_30_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x32D2, 0x00);
	}

	LOG_INF("%s(PD 012515) 30 fpsX\n", __func__);
}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	LOG_INF("%s E! currefps:%d\n", __func__, currefps);
	imx766dual_table_write_cmos_sensor_8(ctx, imx766dual_normal_video_setting,
	sizeof(imx766dual_normal_video_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x32D2, 0x00);
	}

	LOG_INF("X\n");
}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	preview_setting(ctx);

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x32D2, 0x00);
	}
}

static void hdr_write_tri_shutter_w_gph(struct subdrv_ctx *ctx,
		kal_uint16 le, kal_uint16 me, kal_uint16 se, kal_bool gph)
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

	ctx->frame_length = max((kal_uint32)(le + me + se + imgsensor_info.margin),
		ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, imgsensor_info.max_frame_length);

	if (le)
		le = round_up((le-6)/exposure_cnt, 4);
	if (me)
		me = round_up((me-6)/exposure_cnt, 4);
	if (se)
		se = round_up((se-6)/exposure_cnt, 4);

	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x autoflicker_en %d frame_length %d\n",
		le, me, se, ctx->autoflicker_en, ctx->frame_length);

	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x01);

	if (ctx->autoflicker_en) {
		realtime_fps =
			ctx->pclk / ctx->line_length * 10 /
			ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else
			write_frame_len(ctx, ctx->frame_length);
	} else
		write_frame_len(ctx, ctx->frame_length);

	/* Long exposure */
	write_cmos_sensor_8(ctx, 0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, le & 0xFF);
	/* Muddle exposure */
	if (me != 0) {
		/*MID_COARSE_INTEG_TIME[15:8]*/
		write_cmos_sensor_8(ctx, 0x313A, (me >> 8) & 0xFF);
		/*MID_COARSE_INTEG_TIME[7:0]*/
		write_cmos_sensor_8(ctx, 0x313B, me & 0xFF);
	}
	/* Short exposure */
	write_cmos_sensor_8(ctx, 0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0225, se & 0xFF);
	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x00);

	LOG_INF("L! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);
}

static void hdr_write_tri_shutter(struct subdrv_ctx *ctx,
		kal_uint16 le, kal_uint16 me, kal_uint16 se)
{
	hdr_write_tri_shutter_w_gph(ctx, le, me, se, KAL_TRUE);
}


static void hdr_write_tri_gain_w_gph(struct subdrv_ctx *ctx,
		kal_uint16 lg, kal_uint16 mg, kal_uint16 sg, kal_bool gph)
{
	kal_uint16 reg_lg, reg_mg, reg_sg;

	reg_lg = gain2reg(ctx, lg);
	reg_mg = gain2reg(ctx, mg);
	reg_sg = gain2reg(ctx, sg);

	ctx->gain = reg_lg;
	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
	/* Long Gian */
	write_cmos_sensor_8(ctx, 0x0204, (reg_lg>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0205, reg_lg & 0xFF);
	/* Middle Gian */
	if (mg != 0) {
		write_cmos_sensor_8(ctx, 0x313C, (reg_mg>>8) & 0xFF);
		write_cmos_sensor_8(ctx, 0x313D, reg_mg & 0xFF);
	}
	/* Short Gian */
	write_cmos_sensor_8(ctx, 0x0216, (reg_sg>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0217, reg_sg & 0xFF);
	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x00);

	LOG_INF(
		"lg:0x%x, reg_lg:0x%x, mg:0x%x, reg_mg:0x%x, sg:0x%x, reg_sg:0x%x\n",
		lg, reg_lg, mg, reg_mg, sg, reg_sg);
}

static void hdr_write_tri_gain(struct subdrv_ctx *ctx,
		kal_uint16 lg, kal_uint16 mg, kal_uint16 sg)
{
	hdr_write_tri_gain_w_gph(ctx, lg, mg, sg, KAL_TRUE);
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
					write_cmos_sensor_8(ctx, 0x0104, 0x01); \
					write_cmos_sensor_8(ctx, 0x3010, 0x02); \
				} while (0)

#define FMC_GPH_END		do { \
					write_cmos_sensor_8(ctx, 0x0104, 0x00); \
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

static kal_uint32 seamless_switch(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, uint32_t *ae_ctrl)
{
	ctx->extend_frame_length_en = KAL_FALSE;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	{
		kal_uint16 changed_reg_setting[] = {
			PHASE_PIX_OUT_EN, 0x01,
			DOL_EN, 0x00,
			DOL_MODE, 0x00
		};

		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.custom2.pclk;
		ctx->line_length = imgsensor_info.custom2.linelength;
		ctx->frame_length = imgsensor_info.custom2.framelength;
		ctx->min_frame_length = imgsensor_info.custom2.framelength;

		FMC_GPH_START;
		imx766dual_table_write_cmos_sensor_8(ctx, changed_reg_setting,
				sizeof(changed_reg_setting) / sizeof(kal_uint16));

		if (ae_ctrl) {

			LOG_INF("call SENSOR_SCENARIO_ID_NORMAL_PREVIEW %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1]);
			set_shutter_w_gph(ctx, ae_ctrl[SHUTTER_NE_FRM_1], KAL_FALSE);
			set_gain_w_gph(ctx, ae_ctrl[GAIN_NE_FRM_1], KAL_FALSE);
		}
		FMC_GPH_END;
	}
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
	{
		kal_uint16 changed_reg_setting[] = {
			PHASE_PIX_OUT_EN, 0x03,
			DOL_EN, 0x01,
			DOL_MODE, 0x00
		};

		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.custom2.pclk;
		ctx->line_length = imgsensor_info.custom2.linelength;
		ctx->frame_length = imgsensor_info.custom2.framelength;
		ctx->min_frame_length = imgsensor_info.custom2.framelength;

		FMC_GPH_START;
		imx766dual_table_write_cmos_sensor_8(ctx, changed_reg_setting,
				sizeof(changed_reg_setting) / sizeof(kal_uint16));

		if (ae_ctrl) {
			LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM2 %d %d %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1],
					0,
					ae_ctrl[SHUTTER_SE_FRM_1],
					ae_ctrl[GAIN_NE_FRM_1],
					0,
					ae_ctrl[GAIN_SE_FRM_1]);
			hdr_write_tri_shutter_w_gph(ctx,
					ae_ctrl[SHUTTER_NE_FRM_1],
					0,
					ae_ctrl[SHUTTER_SE_FRM_1],
					KAL_FALSE);
			hdr_write_tri_gain_w_gph(ctx,
					ae_ctrl[GAIN_NE_FRM_1],
					0,
					ae_ctrl[GAIN_SE_FRM_1],
					KAL_FALSE);
		}
		FMC_GPH_END;
	}
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
	{
		kal_uint16 changed_reg_setting[] = {
			PHASE_PIX_OUT_EN, 0x07,
			DOL_EN, 0x01,
			DOL_MODE, 0x01
		};

		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.custom3.pclk;
		ctx->line_length = imgsensor_info.custom3.linelength;
		ctx->frame_length = imgsensor_info.custom3.framelength;
		ctx->min_frame_length = imgsensor_info.custom3.framelength;

		FMC_GPH_START;
		imx766dual_table_write_cmos_sensor_8(ctx, changed_reg_setting,
				sizeof(changed_reg_setting) / sizeof(kal_uint16));

		if (ae_ctrl) {
			LOG_INF("call SENSOR_SCENARIO_ID_CUSTOM3 %d %d %d %d %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1],
					ae_ctrl[SHUTTER_ME_FRM_1],
					ae_ctrl[SHUTTER_SE_FRM_1],
					ae_ctrl[GAIN_NE_FRM_1],
					ae_ctrl[GAIN_ME_FRM_1],
					ae_ctrl[GAIN_SE_FRM_1]);
			hdr_write_tri_shutter_w_gph(ctx,
					ae_ctrl[SHUTTER_NE_FRM_1],
					ae_ctrl[SHUTTER_ME_FRM_1],
					ae_ctrl[SHUTTER_SE_FRM_1],
					KAL_FALSE);
			hdr_write_tri_gain_w_gph(ctx,
					ae_ctrl[GAIN_NE_FRM_1],
					ae_ctrl[GAIN_ME_FRM_1],
					ae_ctrl[GAIN_SE_FRM_1],
					KAL_FALSE);
		}
		FMC_GPH_END;
	}
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	{
		kal_uint16 changed_reg_setting[] = {
			PHASE_PIX_OUT_EN, 0x01,
			DOL_EN, 0x00,
			DOL_MODE, 0x00
		};

		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.normal_video.pclk;
		ctx->line_length = imgsensor_info.normal_video.linelength;
		ctx->frame_length = imgsensor_info.normal_video.framelength;
		ctx->min_frame_length = imgsensor_info.normal_video.framelength;

		FMC_GPH_START;
		imx766dual_table_write_cmos_sensor_8(ctx, changed_reg_setting,
				sizeof(changed_reg_setting) / sizeof(kal_uint16));

		if (ae_ctrl) {
			LOG_INF("call SENSOR_SCENARIO_ID_NORMAL_VIDEO %d %d",
					ae_ctrl[SHUTTER_NE_FRM_1], ae_ctrl[GAIN_NE_FRM_1]);
			set_shutter_w_gph(ctx, ae_ctrl[SHUTTER_NE_FRM_1], KAL_FALSE);
			set_gain_w_gph(ctx, ae_ctrl[GAIN_NE_FRM_1], KAL_FALSE);
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

	ctx->fast_mode_on = KAL_TRUE;
	LOG_INF("%s success, scenario is switched to %d", __func__, scenario_id);
	return 0;
}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s E\n", __func__);
	hs_video_setting(ctx);

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x32D2, 0x00);
	}
}

static void custom1_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s 240 fps E! currefps\n", __func__);
	/*************MIPI output setting************/

	imx766dual_table_write_cmos_sensor_8(ctx, imx766dual_custom1_setting,
		sizeof(imx766dual_custom1_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x32D2, 0x00);
	}

	LOG_INF("X");
}

static void custom2_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s 480 fps E! currefps\n", __func__);
	/*************MIPI output setting************/

	imx766dual_table_write_cmos_sensor_8(ctx, imx766dual_custom2_setting,
		sizeof(imx766dual_custom2_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x32D2, 0x00);
	}

	LOG_INF("X");
}

static void custom3_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s 4M*60 fps E! currefps\n", __func__);
	/*************MIPI output setting************/

	imx766dual_table_write_cmos_sensor_8(ctx, imx766dual_custom3_setting,
		sizeof(imx766dual_custom3_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		LOG_INF("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x32D2, 0x00);
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
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8)
					| read_cmos_sensor_8(ctx, 0x0017));
			if (*sensor_id == IMX766_SENSOR_ID) {
				*sensor_id = IMX766DUAL_SENSOR_ID;
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				read_sensor_Cali(ctx);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail. i2c_write_id: 0x%x\n", ctx->i2c_write_id);
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
static int open(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8)
					| read_cmos_sensor_8(ctx, 0x0017));
			if (sensor_id == IMX766_SENSOR_ID) {
				sensor_id = IMX766DUAL_SENSOR_ID;
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
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
	sensor_init(ctx);

	ctx->autoflicker_en = KAL_FALSE;
	ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->shutter = 0x3D0;
	ctx->gain = 0x100;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;
	ctx->current_fps = imgsensor_info.pre.max_framerate;

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
static int close(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");

	/*No Need to implement this function*/

	write_cmos_sensor_8(ctx, 0x0100, 0x00);
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
static kal_uint32 preview(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	preview_setting(ctx);

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
static kal_uint32 capture(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			ctx->current_fps,
			imgsensor_info.cap.max_framerate / 10);
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}

	capture_setting(ctx, ctx->current_fps);

	return ERROR_NONE;
}	/* capture(ctx) */

static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}

	normal_video_setting(ctx, ctx->current_fps);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}

	hs_video_setting(ctx);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}

	slim_video_setting(ctx);

	return ERROR_NONE;
}	/* slim_video */


static kal_uint32 custom1(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}

	custom1_setting(ctx);

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom2.pclk;
	ctx->line_length = imgsensor_info.custom2.linelength;
	ctx->frame_length = imgsensor_info.custom2.framelength;
	ctx->min_frame_length = imgsensor_info.custom2.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}

	custom2_setting(ctx);

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	ctx->pclk = imgsensor_info.custom3.pclk;
	ctx->line_length = imgsensor_info.custom3.linelength;
	ctx->frame_length = imgsensor_info.custom3.framelength;
	ctx->min_frame_length = imgsensor_info.custom3.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}

	custom3_setting(ctx);

	return ERROR_NONE;
}	/* custom3 */

static int get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i = 0;

	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (i < imgsensor_info.sensor_mode_num) {
			sensor_resolution->SensorWidth[i] = imgsensor_winsize_info[i].w2_tg_size;
			sensor_resolution->SensorHeight[i] = imgsensor_winsize_info[i].h2_tg_size;
		} else {
			sensor_resolution->SensorWidth[i] = 0;
			sensor_resolution->SensorHeight[i] = 0;
		}
	}

	return ERROR_NONE;
} /* get_resolution */

static int get_info(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_PREVIEW] =
		imgsensor_info.pre_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_CAPTURE] =
		imgsensor_info.cap_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_VIDEO] =
		imgsensor_info.video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO] =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_SLIM_VIDEO] =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM1] =
		imgsensor_info.custom1_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM2] =
		imgsensor_info.custom2_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM3] =
		imgsensor_info.custom3_delay_frame;

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

	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

	return ERROR_NONE;
}	/*	get_info  */

static int control(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		preview(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		capture(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		normal_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		hs_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		slim_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		custom1(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		custom2(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		custom3(ctx, image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control(ctx) */

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	if ((framerate == 300) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 296;
	else if ((framerate == 150) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 146;
	else
		ctx->current_fps = framerate;
	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	set_max_framerate(ctx, ctx->current_fps, 1);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx, kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	if (enable) /*enable auto flicker*/
		ctx->autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
		ctx->autoflicker_en = KAL_FALSE;
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		ctx->frame_length =
			imgsensor_info.normal_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		if (ctx->current_fps != imgsensor_info.cap.max_framerate)
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

		ctx->dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.cap.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			  ? (frame_length - imgsensor_info.hs_video.framelength)
			  : 0;
		ctx->frame_length =
			imgsensor_info.hs_video.framelength
				+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.slim_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
				/ imgsensor_info.custom1.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom1.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
				/ imgsensor_info.custom2.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom2.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
				/ imgsensor_info.custom3.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
			? (frame_length - imgsensor_info.custom3.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom3.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter) {
			write_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_dummy(ctx);
			write_cmos_sensor_8(ctx, 0x0104, 0x00);
		}
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor_8(ctx, 0x0601, 0x0002); /*100% Color bar*/
	else
		write_cmos_sensor_8(ctx, 0x0601, 0x0000); /*No pattern*/

	ctx->test_pattern = enable;
	return ERROR_NONE;
}

static kal_int32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	UINT8 temperature = 0;
	INT32 temperature_convert = 0;

	temperature = read_cmos_sensor_8(ctx, 0x013a);

	if (temperature >= 0x0 && temperature <= 0x60)
		temperature_convert = temperature;
	else if (temperature >= 0x61 && temperature <= 0x7F)
		temperature_convert = 97;
	else if (temperature >= 0x80 && temperature <= 0xE2)
		temperature_convert = -30;
	else
		temperature_convert = (INT8)temperature | 0xFFFFFF0;

	return temperature_convert;
}

static int feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
				 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
	uint32_t ratio = 1;

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
		*(feature_data + 2) = 24; // 3 exp: 3 * 4 shutter step
		break;
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 3000000;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		if (*(feature_data + 2) & SENSOR_GET_LINELENGTH_FOR_READOUT)
			ratio = get_exp_cnt_by_scenario((*feature_data));

		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ (ratio * imgsensor_info.cap.linelength);
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ (ratio * imgsensor_info.normal_video.linelength);
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ (ratio * imgsensor_info.hs_video.linelength);
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ (ratio * imgsensor_info.slim_video.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ (ratio * imgsensor_info.custom1.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ (ratio * imgsensor_info.custom2.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ (ratio * imgsensor_info.custom3.linelength);
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ (ratio * imgsensor_info.pre.linelength);
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = ctx->line_length;
		*feature_return_para_16 = ctx->frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = ctx->pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		 set_shutter(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(ctx, (UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(ctx, sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(ctx, sensor_reg_data->RegAddr);
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
		set_video_mode(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(ctx, feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(ctx, (BOOL)*feature_data_16,
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(ctx,
				(enum SENSOR_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		 get_default_framerate_by_scenario(ctx,
				(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
		ctx->ihdr_mode = *feature_data_32;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[1],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[3],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[4],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[5],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[6],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[7],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
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
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:

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
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			/* video & capture use same setting */
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH:
		LOG_INF("extend_frame_len %d\n", *feature_data);
		extend_frame_length(ctx, (MUINT32) *feature_data);
		LOG_INF("extend_frame_len done %d\n", *feature_data);
		break;
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
	{
		LOG_INF("SENSOR_FEATURE_SEAMLESS_SWITCH");
		if ((feature_data + 1) != NULL)
			pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		else
			LOG_INF("warning! no ae_ctrl input");

		if (feature_data == NULL) {
			LOG_INF("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}
		LOG_INF("call seamless_switch");
		seamless_switch(ctx, (*feature_data), pAeCtrls);
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*pScenarios = SENSOR_SCENARIO_ID_CUSTOM2;
			*(pScenarios + 1) = SENSOR_SCENARIO_ID_CUSTOM3;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
			*(pScenarios + 1) = SENSOR_SCENARIO_ID_CUSTOM3;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
			*(pScenarios + 1) = SENSOR_SCENARIO_ID_CUSTOM2;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		default:
			*pScenarios = 0xff;
			break;
		}
		LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
				*feature_data, *pScenarios);
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0xB;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0xC;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
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
		if (*feature_data == SENSOR_SCENARIO_ID_NORMAL_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM2;
				break;
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM3;
				break;
			default:
				break;
			}
		} else if (*feature_data == SENSOR_SCENARIO_ID_NORMAL_VIDEO) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM2;
				break;
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM3;
				break;
			default:
				break;
			}
		} else if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM2) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
				break;
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM3;
				break;
			default:
				break;
			}
		} else if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM3) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
				break;
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM2;
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
		if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM2
			|| *feature_data == SENSOR_SCENARIO_ID_CUSTOM3) {
			// see IMX766DUAL SRM, table 5-22 constraints of COARSE_INTEG_TIME
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
			case VC_STAGGER_ME:
			case VC_STAGGER_SE:
			default:
				*(feature_data + 2) = 65532 - imgsensor_info.margin;
				break;
			}
		} else {
			*(feature_data + 2) = 0;
		}
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER://for 2EXP
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
				(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		// implement write shutter for NE/SE
		hdr_write_tri_shutter(ctx, (UINT16)*feature_data,
					0,
					(UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN://for 2EXP
		LOG_INF("SENSOR_FEATURE_SET_DUAL_GAIN LE=%d, SE=%d\n",
				(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		// implement write gain for NE/SE
		hdr_write_tri_gain(ctx, (UINT16)*feature_data,
				   0,
				   (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER://for 3EXP
		LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_SHUTTER LE=%d, ME=%d, SE=%d\n",
				(UINT16) *feature_data,
				(UINT16) *(feature_data + 1),
				(UINT16) *(feature_data + 2));
		hdr_write_tri_shutter(ctx, (UINT16)*feature_data,
				(UINT16) *(feature_data + 1),
				(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN:
		LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_GAIN LG=%d, SG=%d, MG=%d\n",
				(UINT16) *feature_data,
				(UINT16) *(feature_data + 1),
				(UINT16) *(feature_data + 2));
		hdr_write_tri_gain(ctx, (UINT16)*feature_data,
				(UINT16) *(feature_data + 1),
				(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature(ctx);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		imx766dual_get_pdaf_reg_setting(ctx, (*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		imx766dual_set_pdaf_reg_setting(ctx, (*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_INF("PDAF mode :%d\n", *feature_data_16);
		ctx->pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(ctx,
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
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*feature_return_para_32 = 1465;
			break;
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		memcpy(feature_return_para_32,
		&ctx->ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
		break;
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		*feature_return_para_32 = ctx->current_ae_effective_frame;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
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
} /* feature_control(ctx) */

#ifdef IMGSENSOR_VC_ROUTING
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0800,
			.vsize = 0x0600,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0500,
			.vsize = 0x0180,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_2dol[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0800,
			.vsize = 0x0600,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x0800,
			.vsize = 0x0600,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0500,
			.vsize = 0x0180,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x0500,
			.vsize = 0x0180,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_3dol[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0800,
			.vsize = 0x0600,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x0800,
			.vsize = 0x0600,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 0x0800,
			.vsize = 0x0600,
			.user_data_desc = VC_STAGGER_SE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0500,
			.vsize = 0x0180,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x0500,
			.vsize = 0x0180,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x30,
			.hsize = 0x0500,
			.vsize = 0x0180,
			.user_data_desc = VC_PDAF_STATS_SE_PIX_1,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_2dol);
		memcpy(fd->entry, frame_desc_2dol, sizeof(frame_desc_2dol));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_3dol);
		memcpy(fd->entry, frame_desc_3dol, sizeof(frame_desc_3dol));
		break;
	default:
		return -1;
	}

	return 0;
}
#endif

static const struct subdrv_ctx defctx = {

	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_max = BASEGAIN * 64,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_step = 1,
	.exposure_def = 0x3D0,
	.exposure_max = 65532 - 48, /* exposure reg is limited to 4x. max = max - margin */
	.exposure_min = 24,
	.exposure_step = 1,
	.frame_time_delay_frame = 3,
	.margin = 48,
	.max_frame_length = 0xffff,

	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,

	.shutter = 0x3D0,	/* current shutter */
	.gain = BASEGAIN * 4,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34, /* record current sensor's i2c write id */
	.current_ae_effective_frame = 2,
	.extend_frame_length_en = KAL_FALSE,
	.fast_mode_on = KAL_FALSE,
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = get_info,
	.get_resolution = get_resolution,
	.control = control,
	.feature_control = feature_control,
	.close = close,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc = get_frame_desc,
#endif
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_PDN, 0, 0},
	{HW_ID_RST, 0, 1},
	{HW_ID_AVDD, 2800000, 3},
	{HW_ID_AFVDD, 2800000, 3},
	{HW_ID_DVDD, 1100000, 4},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 6},
	{HW_ID_PDN, 1, 0},
	{HW_ID_RST, 1, 5}
};

const struct subdrv_entry imx766dual_mipi_raw_entry = {
	.name = "imx766dual_mipi_raw",
	.id = IMX766DUAL_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};
