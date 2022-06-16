// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX586mipi_Sensor.c
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
#define PFX "IMX586_camera_sensor"
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
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "imx586_ana_gain_table.h"
#include "imx586_seamless_switch.h"
#include "imx586mipiraw_Sensor.h"
#include "imx586_eeprom.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"
#include "adaptor.h"

#define _I2C_BUF_SIZE 4096
static kal_uint16 _i2c_data[_I2C_BUF_SIZE];
static unsigned int _size_to_write;
static bool _is_seamless;

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define imx586_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)

#undef VENDOR_EDIT

#define USE_BURST_MODE 1

#define LONG_EXP 1

#define DPHY_2LANE 0
#define ByPass 0



static void commit_write_sensor(struct subdrv_ctx *ctx)
{
	if (_size_to_write) {
		//pr_debug("commit size:%d",_size_to_write);
		imx586_table_write_cmos_sensor(ctx, _i2c_data, _size_to_write);
		memset(_i2c_data, 0x0, sizeof(_i2c_data));
		_size_to_write = 0;
		//pr_debug("clear size:%d done",_size_to_write);
	}
}

static void set_cmos_sensor_8(struct subdrv_ctx *ctx,
			kal_uint16 reg, kal_uint16 val)
{
	_i2c_data[_size_to_write++] = reg;
	_i2c_data[_size_to_write++] = val;
	if (_size_to_write > _I2C_BUF_SIZE - 2)
		commit_write_sensor(ctx);
}


static kal_uint8 qsc_flag;
static kal_uint8 otp_flag;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX586_SENSOR_ID,

	.checksum_value = 0xa4c32546,

#if DPHY_2LANE
	.pre = { /*48M@9fps*/
		.pclk = 864000000,
		.linelength = 14704,
		.framelength = 6528,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8000,
		.grabwindow_height = 6000,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 499200000,
		.max_framerate = 90,
	},
	.cap = { /*48M@9fps*/
		.pclk = 864000000,
		.linelength = 14704,
		.framelength = 6528,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8000,
		.grabwindow_height = 6000,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 499200000,
		.max_framerate = 90,
	},
#else
	.pre = { /* reg_J 4000x3000 @59.89fps*/
		.pclk = 1488000000,
		.linelength = 7872,
		.framelength = 3156,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 1028570000,
		.max_framerate = 600, /* 60fps */
	},

	.cap = { /*reg_A 12M@30fps*/
		.pclk = 1728000000,
		.linelength = 15744,
		.framelength = 3658,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300, /* 60fps */
	},
#endif

	.normal_video = { /*reg_C-2 4000*2600@30fps*/
		.pclk = 883200000,
		.linelength = 7872,
		.framelength = 3738,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 2600,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 584230000,
		.max_framerate = 300,
	},

	.hs_video = { /* reg_K 1920x1080 @120fps (binning)*/
		.pclk = 960000000,
		.linelength = 4496,
		.framelength = 1778,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 514290000,
		.max_framerate = 1200,
	},

	.slim_video = { /* reg_C-1 4000x2256@30fps */
		.pclk = 883200000,
		.linelength = 7872,
		.framelength = 3738,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 2256,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 584230000,
		.max_framerate = 300,
	},

	.custom1 = { /* reg_E 1920x1080 @240fps*/
		.pclk = 1728000000,
		.linelength = 5376,
		.framelength = 1338,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1644132000,
		.max_framerate = 2400,
	},

	.custom2 = { /* reg_N 4K @ 60fps (binning)*/
		.pclk = 1152000000,
		.linelength = 7872,
		.framelength = 2384,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 867760000,
		.max_framerate = 600,
	},

	.custom3 = { /* reg_E 8000x6000 @30fps*/
		.pclk = 1728000000,
		.linelength = 9440,
		.framelength = 6142,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8000,
		.grabwindow_height = 6000,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.custom4 = { /*reg_A center crop 12M@30fps*/
		.pclk = 1728000000,
		.linelength = 9440,
		.framelength = 6142,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.custom5 = { /*640*480@120fps*/
		.pclk = 1728000000,
		.linelength = 5376,
		.framelength = 2678,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 730970000,
		.max_framerate = 1200, /* 120fps */
	},

	.custom6 = { /* reg_G 8000x4000 @24fps */
		.pclk = 1728000000,
		.linelength = 9440,
		.framelength = 7627,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 7680,
		.grabwindow_height = 4320,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 240,
	},
	.margin = 48,		/* sensor framelength & shutter margin */
	.min_shutter = 6,	/* min shutter */
	.min_gain = 1147, //1024*1.12x(1db)
	.max_gain = 64 * BASEGAIN,//64x
	.min_gain_iso = 100,
	.exp_step = 2,
	.gain_step = 1,
	.gain_type = 0,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 1,/* 1, support; 0,not support */
	.sensor_mode_num = 11,	/* support sensor mode num */

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,	/* enter custom1 delay frame num */
	.custom2_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom3_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom4_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom5_delay_frame = 2,	/* enter custom2 delay frame num */
	.frame_time_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/* .mipi_sensor_type = MIPI_OPHY_NCSI2, */
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
#if DPHY_2LANE
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
#else
	.mipi_sensor_type = MIPI_CPHY, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
#endif
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	/*.mipi_lane_num = SENSOR_MIPI_4_LANE,*/
#if DPHY_2LANE
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
#else
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
#endif
	.i2c_addr_table = {0x34, 0x20, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_speed = 1000, /* i2c read/write speed */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[11] = {
#if DPHY_2LANE
	{8000, 6000, 0,   0, 8000, 6000, 8000, 6000,
	0, 0, 8000, 6000,  0,  0, 8000, 6000}, /* preview */
	{8000, 6000, 0,   0, 8000, 6000, 8000, 6000,
	0, 0, 8000, 6000,  0,  0, 8000, 6000}, /* capture */
#else
	{8000, 6000, 0,   0, 8000, 6000, 4000, 3000,
	0, 0, 4000, 3000,  0,  0, 4000, 3000}, /* Preview */
	{8000, 6000, 0,   0, 8000, 6000, 4000, 3000,
	0, 0, 4000, 3000,  0,  0, 4000, 3000}, /* capture */
#endif
	{8000, 6000, 0, 400, 8000, 5200, 4000, 2600,
	0, 0, 4000, 2600,  0,  0, 4000, 2600}, /* normal video */
	{8000, 6000, 0, 440, 8000, 5120, 2000, 1280,
	40, 76, 1920, 1080,  0,  0, 1920, 1080}, /* hs_video */
	{8000, 6000, 0, 744, 8000, 4512, 4000, 2256,
	0,   0, 4000, 2256,  0,  0, 4000, 2256}, /* slim video */
	{8000, 6000, 0, 744, 8000, 4512, 2000, 1128,
	40,  24, 1920, 1080,  0,  0, 1920, 1080}, /* custom1 */
	{8000, 6000, 0,   0, 8000, 6000, 4000, 3000,
	80, 420, 3840, 2160,  0,  0, 3840, 2160}, /* custom2 */
	{8000, 6000, 0,   0, 8000, 6000, 8000, 6000,
	0,   0, 8000, 6000,  0,  0, 8000, 6000}, /* custom3 */
	{8000, 6000, 2000, 1500, 4000, 3000, 4000, 3000,
	0,  0, 4000, 3000,  0,  0, 4000, 3000}, /* custom4 */
	{8000, 6000, 0,   0, 8000, 6000, 2000, 1500,
	680, 510, 640, 480,  0,  0, 640, 480}, /* custom5 */
	{8000, 6000, 0,   744, 8000, 4512, 8000, 4512,
	160,   96, 7680, 4320,  0,  0, 7680, 4320}, /* custom6 */

};
 /*VC1 for HDR(DT=0X35), VC2 for PDAF(DT=0X36), unit : 10bit */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[9] = {
	/* Preview mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0FA0, 0x0BB8, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x34, 0x04D8, 0x05D0, 0x00, 0x00, 0x0000, 0x0000},
	/* Normal_Video mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0FA0, 0x0a28, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x34, 0x04D8, 0x0510, 0x00, 0x00, 0x0000, 0x0000},
	/* 4K_Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0F00, 0x0870, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x34, 0x04B0, 0x0430, 0x00, 0x00, 0x0000, 0x0000},
	/* Slim_Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0FA0, 0x08d0, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x34, 0x04D8, 0x0460, 0x00, 0x00, 0x0000, 0x0000},
	 /*custom1 setting*/
	 {0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0780, 0x0438, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x34, 0x04D8, 0x05D0, 0x00, 0x00, 0x0000, 0x0000},
	 /*custom3 setting*/
	 {0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1770, 0x1f40, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x34, 0x04D8, 0x05D0, 0x00, 0x00, 0x0000, 0x0000},
	 /*custom4 setting*/
	 {0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0FA0, 0x0BB8, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x026c, 0x02e0, 0x00, 0x00, 0x0000, 0x0000},
	 /*capture setting*/
	 {0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0FA0, 0x0BB8, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x34, 0x04D8, 0x05D0, 0x00, 0x00, 0x0000, 0x0000},
	/*custom6 setting*/
	 {0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1E00, 0x10E0, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x34, 0x04B0, 0x0430, 0x00, 0x00, 0x0000, 0x0000},
};


/* If mirror flip */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_binning = {
	.i4OffsetX = 17,
	.i4OffsetY = 12,
	.i4PitchX  =  8,
	.i4PitchY  = 16,
	.i4PairNum  = 8,
	.i4SubBlkW  = 8,
	.i4SubBlkH  = 2,
	.i4PosL = { {20, 13}, {18, 15}, {22, 17}, {24, 19},
		   {20, 21}, {18, 23}, {22, 25}, {24, 27} },
	.i4PosR = { {19, 13}, {17, 15}, {21, 17}, {23, 19},
		   {19, 21}, {17, 23}, {21, 25}, {23, 27} },
	.i4BlockNumX = 496,
	.i4BlockNumY = 186,
	.iMirrorFlip = 3,
	.i4Crop = { {0, 0}, {0, 0}, {0, 200}, {0, 0}, {0, 372},
		    {0, 0}, {80, 420}, {0, 0}, {0, 0}, {0, 0} },
};

static kal_uint16 imx586_QSC_setting[2304 * 2];
static kal_uint16 imx586_LRC_setting[384 * 2];


static void imx586_get_pdaf_reg_setting(struct subdrv_ctx *ctx, MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(ctx, regDa[idx]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void imx586_set_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor_8(ctx, regDa[idx], regDa[idx + 1]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}

static kal_uint16 read_cmos_eeprom_8(struct subdrv_ctx *ctx, kal_uint16 addr)
{
	u8 val = 0;

	adaptor_i2c_rd_u8(ctx->i2c_client, 0xa0 >> 1, addr, &val);

	return (u16)val;
}

static void read_sensor_Cali(struct subdrv_ctx *ctx)
{
	kal_uint16 idx = 0, addr_qsc = 0xfaf, sensor_lrc = 0x7F00;
	kal_uint16 eeprom_lrc_0 = 0x1620, eeprom_lrc_1 = 0x16E0;
	kal_uint16 sensor_lrc_0 = 0x7510, sensor_lrc_1 = 0x7600;
	kal_uint8 otp_data[9] = {0};
	int i = 0;

	/*read otp data to distinguish module*/
	otp_flag = OTP_QSC_NONE;

	for (i = 0; i < 7; i++)
		otp_data[i] = read_cmos_eeprom_8(ctx, 0x0001 + i);
	/*Internal Module Type*/
	if ((otp_data[0] == 0xff) &&
		(otp_data[1] == 0x00) &&
		(otp_data[2] == 0x0b) &&
		(otp_data[3] == 0x01)) {
		pr_debug("OTP type: Internal Only");
		otp_flag = OTP_QSC_INTERNAL;
		for (idx = 0; idx < 2304; idx++) {
			addr_qsc = 0xfaf + idx;
			sensor_lrc = 0x7F00 + idx;
			imx586_QSC_setting[2 * idx] = sensor_lrc;
			imx586_QSC_setting[2 * idx + 1] =
				read_cmos_eeprom_8(ctx, addr_qsc);
		}
		for (idx = 0; idx < 192; idx++) {
			imx586_LRC_setting[2 * idx] = sensor_lrc_0 + idx;
				imx586_LRC_setting[2 * idx + 1] =
			read_cmos_eeprom_8(ctx, eeprom_lrc_0 + idx);
			imx586_LRC_setting[2 * idx + 192 * 2] =
				sensor_lrc_1 + idx;
			imx586_LRC_setting[2 * idx + 1 + 192 * 2] =
			read_cmos_eeprom_8(ctx, eeprom_lrc_1 + idx);
		}
	} else if ((otp_data[5] == 0x56) && (otp_data[6] == 0x00)) {
		/*Internal Module Type*/
		pr_debug("OTP type: Custom Only");
		otp_flag = OTP_QSC_CUSTOM;
		for (idx = 0; idx < 2304; idx++) {
			addr_qsc = 0xc90 + idx;
			sensor_lrc = 0x7F00 + idx;
			imx586_QSC_setting[2 * idx] = sensor_lrc;
			imx586_QSC_setting[2 * idx + 1] =
				read_cmos_eeprom_8(ctx, addr_qsc);
		}
	} else {
		pr_debug("OTP type: No Data, 0x0008 = %d, 0x0009 = %d",
		read_cmos_eeprom_8(ctx, 0x0008),
		read_cmos_eeprom_8(ctx, 0x0009));
	}
	ctx->is_read_preload_eeprom = 1;
}

static void write_sensor_QSC(struct subdrv_ctx *ctx)
{
	if ((otp_flag == OTP_QSC_CUSTOM) || (otp_flag == OTP_QSC_INTERNAL)) {
		imx586_table_write_cmos_sensor(ctx, imx586_QSC_setting,
		sizeof(imx586_QSC_setting) / sizeof(kal_uint16));
	}
}

static void set_dummy(struct subdrv_ctx *ctx)
{
	DEBUG_LOG(ctx, "dummyline = %d, dummypixels = %d\n",
		ctx->dummy_line, ctx->dummy_pixel);
	/* return;*/ /* for test */
	if (!_is_seamless) {
		set_cmos_sensor_8(ctx, 0x0104, 0x01);

		set_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
		set_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
		//set_cmos_sensor_8(ctx, 0x0342, ctx->line_length >> 8);
		//set_cmos_sensor_8(ctx, 0x0343, ctx->line_length & 0xFF);

		set_cmos_sensor_8(ctx, 0x0104, 0x00);

		commit_write_sensor(ctx);
	//write_cmos_sensor_8(ctx, 0x0104, 0x01);
	} else { /*wait to check*/
		_i2c_data[_size_to_write++] = 0x0340;
		_i2c_data[_size_to_write++] = ctx->frame_length >> 8;
		_i2c_data[_size_to_write++] = 0x0341;
		_i2c_data[_size_to_write++] = ctx->frame_length & 0xFF;
	}
}	/*	set_dummy  */

static void set_mirror_flip(struct subdrv_ctx *ctx, kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	pr_debug("image_mirror = %d\n", image_mirror);
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

static void set_max_framerate(struct subdrv_ctx *ctx,
		UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = ctx->frame_length;

	DEBUG_LOG(ctx,
		"framerate = %d, min framelength should enable %d\n", framerate,
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
}	/*	set_max_framerate  */

static void set_max_framerate_video(struct subdrv_ctx *ctx, UINT16 framerate,
					kal_bool min_framelength_en)
{
	set_max_framerate(ctx, framerate, min_framelength_en);
	set_dummy(ctx);
}


static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	#ifdef LONG_EXP
	/*Yijun.Tan@camera.driver,20180116,add for slow shutter */
	int longexposure_times = 0;
	static int long_exposure_status;
	#endif

	//if (shutter > ctx->min_frame_length - imgsensor_info.margin)
	//	ctx->frame_length = shutter + imgsensor_info.margin;
	//else
	ctx->frame_length = ctx->min_frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10
				/ ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
	}
	if (!_is_seamless)
		set_cmos_sensor_8(ctx, 0x0104, 0x01);
#ifdef LONG_EXP
	while (shutter >= 65535) {
		shutter = shutter / 2;
		longexposure_times += 1;
	}
	if (!_is_seamless)
		if (read_cmos_sensor_8(ctx, 0x0350) != 0x01) {
			pr_info("single cam scenario enable auto-extend");
			set_cmos_sensor_8(ctx, 0x0350, 0x01);
		}
	if (longexposure_times > 0) {
		pr_debug("enter long exposure mode, time is %d",
			longexposure_times);
		long_exposure_status = 1;
		//ctx->frame_length = shutter + 32;
		if (!_is_seamless)
			set_cmos_sensor_8(ctx, 0x3100, longexposure_times & 0x07);
		else {
			_i2c_data[_size_to_write++] = 0x3100;
			_i2c_data[_size_to_write++] = longexposure_times & 0x07;
		}
	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;

		if (!_is_seamless)
			set_cmos_sensor_8(ctx, 0x3100, 0x00);
		else {
			_i2c_data[_size_to_write++] = 0x3100;
			_i2c_data[_size_to_write++] = longexposure_times & 0x00;
		}

		LOG_INF("exit long exposure mode");
	}
	if (!_is_seamless) {
		set_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
		set_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	} else {
		_i2c_data[_size_to_write++] = 0x0340;
		_i2c_data[_size_to_write++] = ctx->frame_length >> 8;
		_i2c_data[_size_to_write++] = 0x0341;
		_i2c_data[_size_to_write++] = ctx->frame_length & 0xFF;
	}
#endif
	/* Update Shutter */
	if (!_is_seamless) {
		set_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
		set_cmos_sensor_8(ctx, 0x0203, shutter  & 0xFF);
		if (!ctx->ae_ctrl_gph_en)
			set_cmos_sensor_8(ctx, 0x0104, 0x00);
		commit_write_sensor(ctx);
	} else {
		_i2c_data[_size_to_write++] = 0x0202;
		_i2c_data[_size_to_write++] = (shutter >> 8) & 0xFF;
		_i2c_data[_size_to_write++] = 0x0203;
		_i2c_data[_size_to_write++] = shutter & 0xFF;
	}
	DEBUG_LOG(ctx, "shutter =%d, framelength =%d\n",
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
static void set_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	ctx->shutter = shutter;

	write_shutter(ctx, shutter);
} /* set_shutter */


static void set_frame_length(struct subdrv_ctx *ctx, kal_uint16 frame_length)
{
	if (frame_length > 1)
		ctx->frame_length = frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (ctx->min_frame_length > ctx->frame_length)
		ctx->frame_length = ctx->min_frame_length;

	/* Extend frame length */
	set_cmos_sensor_8(ctx, 0x0104, 0x01);
	set_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
	set_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	set_cmos_sensor_8(ctx, 0x0104, 0x00);

	commit_write_sensor(ctx);

	pr_debug("Framelength: set=%d/input=%d/min=%d, auto_extend=%d\n",
		ctx->frame_length, frame_length, ctx->min_frame_length,
		read_cmos_sensor_8(ctx, 0x0350));
}


static void set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint32 *shutters, kal_uint16 shutter_cnt,
				kal_uint16 frame_length)
{
	if (shutter_cnt == 1) {
		ctx->shutter = shutters[0];

		//if (shutters[0] > ctx->min_frame_length - imgsensor_info.margin)
		//	ctx->frame_length = shutters[0] + imgsensor_info.margin;
		//else
		ctx->frame_length = ctx->min_frame_length;
		if (frame_length > ctx->frame_length)
			ctx->frame_length = frame_length;
		if (ctx->frame_length > imgsensor_info.max_frame_length)
			ctx->frame_length = imgsensor_info.max_frame_length;
		if (shutters[0] < imgsensor_info.min_shutter)
			shutters[0] = imgsensor_info.min_shutter;

		if (!_is_seamless) {
			set_cmos_sensor_8(ctx, 0x0104, 0x01);
			set_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
			set_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);

			set_cmos_sensor_8(ctx, 0x0202, (shutters[0] >> 8) & 0xFF);
			set_cmos_sensor_8(ctx, 0x0203, shutters[0]  & 0xFF);
			if (!ctx->ae_ctrl_gph_en)
				set_cmos_sensor_8(ctx, 0x0104, 0x00);
			commit_write_sensor(ctx);
		} else {
			_i2c_data[_size_to_write++] = 0x0340;
			_i2c_data[_size_to_write++] = ctx->frame_length >> 8;
			_i2c_data[_size_to_write++] = 0x0341;
			_i2c_data[_size_to_write++] = ctx->frame_length & 0xFF;

			_i2c_data[_size_to_write++] = 0x0202;
			_i2c_data[_size_to_write++] = (shutters[0] >> 8) & 0xFF;
			_i2c_data[_size_to_write++] = 0x0203;
			_i2c_data[_size_to_write++] = shutters[0] & 0xFF;
		}

		DEBUG_LOG(ctx, "shutter =%d, framelength =%d\n",
			shutters[0], ctx->frame_length);
	}
}

/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(struct subdrv_ctx *ctx, kal_uint16 shutter,
				     kal_uint16 frame_length,
				     kal_bool auto_extend_en)
{
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	ctx->shutter = shutter;

	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;

	ctx->frame_length = ctx->frame_length + dummy_line;

	//if (shutter > ctx->frame_length - imgsensor_info.margin)
	//	ctx->frame_length = shutter + imgsensor_info.margin;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10 /
				ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
	}

	/* Update Shutter */
	set_cmos_sensor_8(ctx, 0x0104, 0x01);
	if (auto_extend_en)
		set_cmos_sensor_8(ctx, 0x0350, 0x01); /* Enable auto extend */
	else
		set_cmos_sensor_8(ctx, 0x0350, 0x00); /* Disable auto extend */
	set_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
	set_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	set_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0203, shutter  & 0xFF);
	set_cmos_sensor_8(ctx, 0x0104, 0x00);

	commit_write_sensor(ctx);
	pr_debug(
		"Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, ctx->frame_length, frame_length,
		dummy_line, read_cmos_sensor_8(ctx, 0x0350));

}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = 1024 - (1024 * BASEGAIN) / gain;
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
static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{
	kal_uint16 reg_gain;
	kal_uint32 min_gain, max_gain;

	min_gain = BASEGAIN;
	max_gain = imgsensor_info.max_gain;

	//16x for full size mode
	switch (ctx->sensor_mode) {
	case IMGSENSOR_MODE_CUSTOM3:
	case IMGSENSOR_MODE_CUSTOM4:
	case IMGSENSOR_MODE_CUSTOM6:
		max_gain = 16 * BASEGAIN;
		break;
	default:
		break;
	}

	if (gain < min_gain || gain > max_gain) {
		pr_debug("Error max gain setting: %d Should between %d & %d\n",
			gain, min_gain, max_gain);
		if (gain < min_gain)
			gain = min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	DEBUG_LOG(ctx, "gain = %d, reg_gain = 0x%x, max_gain:%d\n ",
		gain, reg_gain, max_gain);
	if (!_is_seamless) {
		if (!ctx->ae_ctrl_gph_en)
			set_cmos_sensor_8(ctx, 0x0104, 0x01);
		set_cmos_sensor_8(ctx, 0x0204, (reg_gain>>8) & 0xFF);
		set_cmos_sensor_8(ctx, 0x0205, reg_gain & 0xFF);
		set_cmos_sensor_8(ctx, 0x0104, 0x00);

		commit_write_sensor(ctx);
	} else {
		_i2c_data[_size_to_write++] = 0x0204;
		_i2c_data[_size_to_write++] =  (reg_gain>>8) & 0xFF;
		_i2c_data[_size_to_write++] = 0x0205;
		_i2c_data[_size_to_write++] = reg_gain & 0xFF;
	}

	return gain;
} /* set_gain */

static kal_uint32 imx586_awb_gain(struct subdrv_ctx *ctx,
		struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	return ERROR_NONE;
}

static kal_uint16 imx586_feedback_awbgain[] = {
	0x0b90, 0x00,
	0x0b91, 0x01,
	0x0b92, 0x00,
	0x0b93, 0x01,
};
/*write AWB gain to sensor*/
static void feedback_awbgain(struct subdrv_ctx *ctx,
		kal_uint32 r_gain, kal_uint32 b_gain)
{
	UINT32 r_gain_int = 0;
	UINT32 b_gain_int = 0;

	r_gain_int = r_gain / 512;
	b_gain_int = b_gain / 512;

	imx586_feedback_awbgain[1] = r_gain_int;
	imx586_feedback_awbgain[3] = (
		((r_gain*100) / 512) - (r_gain_int * 100)) * 2;
	imx586_feedback_awbgain[5] = b_gain_int;
	imx586_feedback_awbgain[7] = (
		((b_gain * 100) / 512) - (b_gain_int * 100)) * 2;
	imx586_table_write_cmos_sensor(ctx, imx586_feedback_awbgain,
		sizeof(imx586_feedback_awbgain)/sizeof(kal_uint16));

}

static void imx586_set_lsc_reg_setting(struct subdrv_ctx *ctx,
		kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
	int i;
	int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};
	/*0:B,1:Gb,2:Gr,3:R*/

	pr_debug("E! index:%d, regNum:%d\n", index, regNum);

	write_cmos_sensor_8(ctx, 0x0B00, 0x01); /*lsc enable*/
	write_cmos_sensor_8(ctx, 0x9014, 0x01);
	write_cmos_sensor_8(ctx, 0x4439, 0x01);
	mdelay(1);
	pr_debug("Addr 0xB870, 0x380D Value:0x%x %x\n",
		read_cmos_sensor_8(ctx, 0xB870),
		read_cmos_sensor_8(ctx, 0x380D));
	/*define Knot point, 2'b01:u3.7*/
	write_cmos_sensor_8(ctx, 0x9750, 0x01);
	write_cmos_sensor_8(ctx, 0x9751, 0x01);
	write_cmos_sensor_8(ctx, 0x9752, 0x01);
	write_cmos_sensor_8(ctx, 0x9753, 0x01);

	for (i = 0; i < regNum; i++)
		write_cmos_sensor(ctx, startAddr[index] + 2*i, regDa[i]);

	write_cmos_sensor_8(ctx, 0x0B00, 0x00); /*lsc disable*/
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
static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	pr_debug(
		"%s streaming: %d x%08x|0x%08x|0x%08x|0x%08x|0x%08x|0x%08x|0x%08x|0x%08x|0x%08x|0x%08x|0x%08x\n",
		__func__,
		enable,
		read_cmos_sensor_8(ctx, 0x0808),
		read_cmos_sensor_8(ctx, 0x084E),
		read_cmos_sensor_8(ctx, 0x084F),
		read_cmos_sensor_8(ctx, 0x0850),
		read_cmos_sensor_8(ctx, 0x0851),
		read_cmos_sensor_8(ctx, 0x0852),
		read_cmos_sensor_8(ctx, 0x0853),
		read_cmos_sensor_8(ctx, 0x0854),
		read_cmos_sensor_8(ctx, 0x0855),
		read_cmos_sensor_8(ctx, 0x0858),
		read_cmos_sensor_8(ctx, 0x0859));

	if (enable) {
		//test pattern reset
		if (read_cmos_sensor_8(ctx, 0x0350) != 0x01) {
			pr_debug("single cam scenario enable auto-extend");
			write_cmos_sensor_8(ctx, 0x0350, 0x01);
		}
		if (_is_seamless)
			write_cmos_sensor_8(ctx, 0x3020, 0x00);/*Mode transition mode change*/
		write_cmos_sensor_8(ctx, 0x0100, 0X01);
		ctx->is_streaming = true;
	} else {
		write_cmos_sensor_8(ctx, 0x0100, 0x00);
		// write_cmos_sensor_8(ctx, 0x0808, 0x00);// set mipi timing back to auto

		ctx->is_streaming = false;
	}
	return ERROR_NONE;
}

static kal_uint16 imx586_init_setting[] = {
#if DPHY_2LANE
	0x0136, 0x18,
	0x0137, 0x00,
	0x3C7E, 0x02,
	0x3C7F, 0x0A,
	0x0111, 0x02,
	0x380C, 0x00,
	0x3C00, 0x10,
	0x3C01, 0x10,
	0x3C02, 0x10,
	0x3C03, 0x10,
	0x3C04, 0x10,
	0x3C05, 0x01,
	0x3C06, 0x00,
	0x3C07, 0x00,
	0x3C08, 0x03,
	0x3C09, 0xFF,
	0x3C0A, 0x01,
	0x3C0B, 0x00,
	0x3C0C, 0x00,
	0x3C0D, 0x03,
	0x3C0E, 0xFF,
	0x3C0F, 0x20,
	0x3F88, 0x00,
	0x3F8E, 0x00,
	0x5282, 0x01,
	0x9004, 0x14,
	0x9200, 0xF4,
	0x9201, 0xA7,
	0x9202, 0xF4,
	0x9203, 0xAA,
	0x9204, 0xF4,
	0x9205, 0xAD,
	0x9206, 0xF4,
	0x9207, 0xB0,
	0x9208, 0xF4,
	0x9209, 0xB3,
	0x920A, 0xB7,
	0x920B, 0x34,
	0x920C, 0xB7,
	0x920D, 0x36,
	0x920E, 0xB7,
	0x920F, 0x37,
	0x9210, 0xB7,
	0x9211, 0x38,
	0x9212, 0xB7,
	0x9213, 0x39,
	0x9214, 0xB7,
	0x9215, 0x3A,
	0x9216, 0xB7,
	0x9217, 0x3C,
	0x9218, 0xB7,
	0x9219, 0x3D,
	0x921A, 0xB7,
	0x921B, 0x3E,
	0x921C, 0xB7,
	0x921D, 0x3F,
	0x921E, 0x77,
	0x921F, 0x77,
	0x9222, 0xC4,
	0x9223, 0x4B,
	0x9224, 0xC4,
	0x9225, 0x4C,
	0x9226, 0xC4,
	0x9227, 0x4D,
	0x9810, 0x14,
	0x9814, 0x14,
	0x99B2, 0x20,
	0x99B3, 0x0F,
	0x99B4, 0x0F,
	0x99B5, 0x0F,
	0x99B6, 0x0F,
	0x99E4, 0x0F,
	0x99E5, 0x0F,
	0x99E6, 0x0F,
	0x99E7, 0x0F,
	0x99E8, 0x0F,
	0x99E9, 0x0F,
	0x99EA, 0x0F,
	0x99EB, 0x0F,
	0x99EC, 0x0F,
	0x99ED, 0x0F,
	0xA569, 0x06,
	0xA56A, 0x13,
	0xA56B, 0x13,
	0xA679, 0x20,
	0xA830, 0x68,
	0xA831, 0x56,
	0xA832, 0x2B,
	0xA833, 0x55,
	0xA834, 0x55,
	0xA835, 0x16,
	0xA837, 0x51,
	0xA838, 0x34,
	0xA854, 0x4F,
	0xA855, 0x48,
	0xA856, 0x45,
	0xA857, 0x02,
	0xA85A, 0x23,
	0xA85B, 0x16,
	0xA85C, 0x12,
	0xA85D, 0x02,
	0xAC72, 0x01,
	0xAC73, 0x26,
	0xAC74, 0x01,
	0xAC75, 0x26,
	0xAC76, 0x00,
	0xAC77, 0xC4,
	0xB051, 0x02,
	0xC020, 0x01,
	0xC61D, 0x00,
	0xC625, 0x00,
	0xC638, 0x03,
	0xC63B, 0x01,
	0xE286, 0x31,
	0xE2A6, 0x32,
	0xE2C6, 0x33,
	0xEA4B, 0x00,
	0xEA4C, 0x00,
	0xEA4D, 0x00,
	0xEA4E, 0x00,
	0xF000, 0x00,
	0xF001, 0x10,
	0xF00C, 0x00,
	0xF00D, 0x40,
	0xF030, 0x00,
	0xF031, 0x10,
	0xF03C, 0x00,
	0xF03D, 0x40,
	0xF44B, 0x80,
	0xF44C, 0x10,
	0xF44D, 0x06,
	0xF44E, 0x80,
	0xF44F, 0x10,
	0xF450, 0x06,
	0xF451, 0x80,
	0xF452, 0x10,
	0xF453, 0x06,
	0xF454, 0x80,
	0xF455, 0x10,
	0xF456, 0x06,
	0xF457, 0x80,
	0xF458, 0x10,
	0xF459, 0x06,
	0xF478, 0x20,
	0xF479, 0x80,
	0xF47A, 0x80,
	0xF47B, 0x20,
	0xF47C, 0x80,
	0xF47D, 0x80,
	0xF47E, 0x20,
	0xF47F, 0x80,
	0xF480, 0x80,
	0xF481, 0x20,
	0xF482, 0x60,
	0xF483, 0x80,
	0xF484, 0x20,
	0xF485, 0x60,
	0xF486, 0x80,
	0x9852, 0x00,
	0x9954, 0x0F,
	0xA7AD, 0x01,
	0xA7CB, 0x01,
	0xAE09, 0xFF,
	0xAE0A, 0xFF,
	0xAE12, 0x58,
	0xAE13, 0x58,
	0xAE15, 0x10,
	0xAE16, 0x10,
	0xAF05, 0x48,
	0xB07C, 0x02,
#else
	/*External Clock Setting*/
	0x0136, 0x18,
	0x0137, 0x00,
	/*Register version*/
	0x3C7E, 0x08,
	0x3C7F, 0x0A,
	/*Signaling mode setting*/
	0x0111, 0x03,
	/*Global Setting*/
	0x380C, 0x00,
	0x3C00, 0x10,
	0x3C01, 0x10,
	0x3C02, 0x10,
	0x3C03, 0x10,
	0x3C04, 0x10,
	0x3C05, 0x01,
	0x3C06, 0x00,
	0x3C07, 0x00,
	0x3C08, 0x03,
	0x3C09, 0xFF,
	0x3C0A, 0x01,
	0x3C0B, 0x00,
	0x3C0C, 0x00,
	0x3C0D, 0x03,
	0x3C0E, 0xFF,
	0x3C0F, 0x20,
	0x3F88, 0x00,
	0x3F8E, 0x00,
	0x5282, 0x01,
	0x9004, 0x14,
	0x9200, 0xF4,
	0x9201, 0xA7,
	0x9202, 0xF4,
	0x9203, 0xAA,
	0x9204, 0xF4,
	0x9205, 0xAD,
	0x9206, 0xF4,
	0x9207, 0xB0,
	0x9208, 0xF4,
	0x9209, 0xB3,
	0x920A, 0xB7,
	0x920B, 0x34,
	0x920C, 0xB7,
	0x920D, 0x36,
	0x920E, 0xB7,
	0x920F, 0x37,
	0x9210, 0xB7,
	0x9211, 0x38,
	0x9212, 0xB7,
	0x9213, 0x39,
	0x9214, 0xB7,
	0x9215, 0x3A,
	0x9216, 0xB7,
	0x9217, 0x3C,
	0x9218, 0xB7,
	0x9219, 0x3D,
	0x921A, 0xB7,
	0x921B, 0x3E,
	0x921C, 0xB7,
	0x921D, 0x3F,
	0x921E, 0x77,
	0x921F, 0x77,
	0x9222, 0xC4,
	0x9223, 0x4B,
	0x9224, 0xC4,
	0x9225, 0x4C,
	0x9226, 0xC4,
	0x9227, 0x4D,
	0x9810, 0x14,
	0x9814, 0x14,
	0x99B2, 0x20,
	0x99B3, 0x0F,
	0x99B4, 0x0F,
	0x99B5, 0x0F,
	0x99B6, 0x0F,
	0x99E4, 0x0F,
	0x99E5, 0x0F,
	0x99E6, 0x0F,
	0x99E7, 0x0F,
	0x99E8, 0x0F,
	0x99E9, 0x0F,
	0x99EA, 0x0F,
	0x99EB, 0x0F,
	0x99EC, 0x0F,
	0x99ED, 0x0F,
	0xA569, 0x06,
	0xA56A, 0x13,
	0xA56B, 0x13,
	0xA679, 0x20,
	0xA830, 0x68,
	0xA831, 0x56,
	0xA832, 0x2B,
	0xA833, 0x55,
	0xA834, 0x55,
	0xA835, 0x16,
	0xA837, 0x51,
	0xA838, 0x34,
	0xA854, 0x4F,
	0xA855, 0x48,
	0xA856, 0x45,
	0xA857, 0x02,
	0xA85A, 0x23,
	0xA85B, 0x16,
	0xA85C, 0x12,
	0xA85D, 0x02,
	0xAC72, 0x01,
	0xAC73, 0x26,
	0xAC74, 0x01,
	0xAC75, 0x26,
	0xAC76, 0x00,
	0xAC77, 0xC4,
	0xB051, 0x02,
	0xC020, 0x01,
	0xC61D, 0x00,
	0xC625, 0x00,
	0xC638, 0x03,
	0xC63B, 0x01,
	0xE286, 0x31,
	0xE2A6, 0x32,
	0xE2C6, 0x33,
	0xEA4B, 0x00,
	0xEA4C, 0x00,
	0xEA4D, 0x00,
	0xEA4E, 0x00,
	0xF000, 0x00,
	0xF001, 0x10,
	0xF00C, 0x00,
	0xF00D, 0x40,
	0xF030, 0x00,
	0xF031, 0x10,
	0xF03C, 0x00,
	0xF03D, 0x40,
	0xF44B, 0x80,
	0xF44C, 0x10,
	0xF44D, 0x06,
	0xF44E, 0x80,
	0xF44F, 0x10,
	0xF450, 0x06,
	0xF451, 0x80,
	0xF452, 0x10,
	0xF453, 0x06,
	0xF454, 0x80,
	0xF455, 0x10,
	0xF456, 0x06,
	0xF457, 0x80,
	0xF458, 0x10,
	0xF459, 0x06,
	0xF478, 0x20,
	0xF479, 0x80,
	0xF47A, 0x80,
	0xF47B, 0x20,
	0xF47C, 0x80,
	0xF47D, 0x80,
	0xF47E, 0x20,
	0xF47F, 0x80,
	0xF480, 0x80,
	0xF481, 0x20,
	0xF482, 0x60,
	0xF483, 0x80,
	0xF484, 0x20,
	0xF485, 0x60,
	0xF486, 0x80,
#endif
};

static kal_uint16 imx586_capture_setting[] = {
#if DPHY_2LANE
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x01,
	0x0342, 0x39,
	0x0343, 0x70,
	0x0340, 0x19,
	0x0341, 0x80,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x17,
	0x034B, 0x6F,
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x3140, 0x00,
	0x3246, 0x01,
	0x3247, 0x01,
	0x3F15, 0x00,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x1F,
	0x040D, 0x40,
	0x040E, 0x17,
	0x040F, 0x70,
	0x034C, 0x1F,
	0x034D, 0x40,
	0x034E, 0x17,
	0x034F, 0x70,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x68,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0xA0,
	0x0310, 0x01,
	0x3620, 0x01,
	0x3621, 0x01,
	0x3C11, 0x08,
	0x3C12, 0x08,
	0x3C13, 0x2A,
	0x3F0C, 0x00,
	0x3F14, 0x01,
	0x3F80, 0x00,
	0x3F81, 0x14,
	0x3F8C, 0x00,
	0x3F8D, 0x14,
	0x3FF8, 0x00,
	0x3FF9, 0x00,
	0x3FFE, 0x03,
	0x3FFF, 0x52,
	0x0202, 0x19,
	0x0203, 0x50,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	0x3E20, 0x01,
	0x3E37, 0x00,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x3d,
	0x0343, 0x80,
	/*Frame Length Lines Setting*/
	0x0340, 0x0e,
	0x0341, 0x4a,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x17,
	0x034B, 0x6F,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3140, 0x00,
	0x3246, 0x81,
	0x3247, 0x81,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0F,
	0x040D, 0xA0,
	0x040E, 0x0B,
	0x040F, 0xB8,
	/*Output Size Setting*/
	0x034C, 0x0F,
	0x034D, 0xA0,
	0x034E, 0x0B,
	0x034F, 0xB8,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x68,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0xa0,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x04,
	0x3C12, 0x03,
	0x3C13, 0x2D,
	0x3F0C, 0x01,
	0x3F14, 0x00,
	0x3F80, 0x00,
	0x3F81, 0x14,
	0x3F8C, 0x00,
	0x3F8D, 0x14,
	0x3FF8, 0x01,
	0x3FF9, 0x90,
	0x3FFE, 0x02,
	0x3FFF, 0xf8,
	/*Integration Setting*/
	0x0202, 0x0e,
	0x0203, 0x1a,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0xF0,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x12,
	0x0852, 0x00,
	0x0853, 0x25,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

#endif
};


static kal_uint16 imx586_preview_setting[] = {
#if DPHY_2LANE
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x01,
	0x0342, 0x39,
	0x0343, 0x70,
	0x0340, 0x19,
	0x0341, 0x80,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x17,
	0x034B, 0x6F,
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x3140, 0x00,
	0x3246, 0x01,
	0x3247, 0x01,
	0x3F15, 0x00,
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x1F,
	0x040D, 0x40,
	0x040E, 0x17,
	0x040F, 0x70,
	0x034C, 0x1F,
	0x034D, 0x40,
	0x034E, 0x17,
	0x034F, 0x70,
	0x0301, 0x05,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x68,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0xA0,
	0x0310, 0x01,
	0x3620, 0x01,
	0x3621, 0x01,
	0x3C11, 0x08,
	0x3C12, 0x08,
	0x3C13, 0x2A,
	0x3F0C, 0x00,
	0x3F14, 0x01,
	0x3F80, 0x00,
	0x3F81, 0x14,
	0x3F8C, 0x00,
	0x3F8D, 0x14,
	0x3FF8, 0x00,
	0x3FF9, 0x00,
	0x3FFE, 0x03,
	0x3FFF, 0x52,
	0x0202, 0x19,
	0x0203, 0x50,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	0x3E20, 0x01,
	0x3E37, 0x00,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xC0,
	/*Frame Length Lines Setting*/
	0x0340, 0x0c,
	0x0341, 0x54,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x17,
	0x034B, 0x6F,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3140, 0x00,
	0x3246, 0x81,
	0x3247, 0x81,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0F,
	0x040D, 0xA0,
	0x040E, 0x0B,
	0x040F, 0xB8,
	/*Output Size Setting*/
	0x034C, 0x0F,
	0x034D, 0xA0,
	0x034E, 0x0B,
	0x034F, 0xB8,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x9B,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0xFA,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x04,
	0x3C12, 0x03,
	0x3C13, 0x2D,
	0x3F0C, 0x01,
	0x3F14, 0x00,
	0x3F80, 0x01,
	0x3F81, 0x90,
	0x3F8C, 0x00,
	0x3F8D, 0x14,
	0x3FF8, 0x01,
	0x3FF9, 0x2A,
	0x3FFE, 0x00,
	0x3FFF, 0x6C,
	/*Integration Setting*/
	0x0202, 0x0c,
	0x0203, 0x24,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0xF0,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x12,
	0x0850, 0x00,
	0x0851, 0x0F,
	0x0852, 0x00,
	0x0853, 0x1E,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

#endif
};


static kal_uint16 imx586_normal_video_setting_4K60FPS[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xC0,
	/*Frame Length Lines Setting*/
	0x0340, 0x09,
	0x0341, 0x50,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0xE8,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x14,
	0x034B, 0x87,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3140, 0x00,
	0x3246, 0x81,
	0x3247, 0x81,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x50,
	0x040A, 0x00,
	0x040B, 0x30,
	0x040C, 0x0F,
	0x040D, 0x00,
	0x040E, 0x08,
	0x040F, 0x70,
	/*Output Size Setting*/
	0x034C, 0x0F,
	0x034D, 0x00,
	0x034E, 0x08,
	0x034F, 0x70,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xF0,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0xD8,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x04,
	0x3C12, 0x03,
	0x3C13, 0x2D,
	0x3F0C, 0x01,
	0x3F14, 0x00,
	0x3F80, 0x01,
	0x3F81, 0x90,
	0x3F8C, 0x00,
	0x3F8D, 0x14,
	0x3FF8, 0x01,
	0x3FF9, 0x2A,
	0x3FFE, 0x00,
	0x3FFF, 0x6C,
	/*Integration Setting*/
	0x0202, 0x09,
	0x0203, 0x20,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0xE0,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x0C,
	0x0850, 0x00,
	0x0851, 0x0A,
	0x0852, 0x00,
	0x0853, 0x14,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};

static kal_uint16 imx586_normal_video_setting[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xC0,
	/*Frame Length Lines Setting*/
	0x0340, 0x0E,
	0x0341, 0x9A,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0x90,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x15,
	0x034B, 0xDF,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3140, 0x00,
	0x3246, 0x81,
	0x3247, 0x81,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0F,
	0x040D, 0xA0,
	0x040E, 0x0A,
	0x040F, 0x28,
	/*Output Size Setting*/
	0x034C, 0x0F,
	0x034D, 0xA0,
	0x034E, 0x0A,
	0x034F, 0x28,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB8,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0x1C,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x04,
	0x3C12, 0x03,
	0x3C13, 0x2D,
	0x3F0C, 0x01,
	0x3F14, 0x00,
	0x3F80, 0x01,
	0x3F81, 0x90,
	0x3F8C, 0x00,
	0x3F8D, 0x14,
	0x3FF8, 0x01,
	0x3FF9, 0x2A,
	0x3FFE, 0x00,
	0x3FFF, 0x6C,
	/*Integration Setting*/
	0x0202, 0x0E,
	0x0203, 0x6A,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0xF0,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x08,
	0x0850, 0x00,
	0x0851, 0x07,
	0x0852, 0x00,
	0x0853, 0x0E,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x05,
	0x0850, 0x00,
	0x0851, 0x04,
	0x0852, 0x00,
	0x0853, 0x09,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};

static kal_uint16 imx586_hs_video_setting[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x11,
	0x0343, 0x90,
	/*Frame Length Lines Setting*/
	0x0340, 0x06,
	0x0341, 0xf2,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0xE8,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x14,
	0x034B, 0x87,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x0a,
	0x3140, 0x00,
	0x3246, 0x89,
	0x3247, 0x89,
	0x3F15, 0x01,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x28,
	0x040A, 0x00,
	0x040B, 0x18,
	0x040C, 0x07,
	0x040D, 0x80,
	0x040E, 0x04,
	0x040F, 0x38,
	/*Output Size Setting*/
	0x034C, 0x07,
	0x034D, 0x80,
	0x034E, 0x04,
	0x034F, 0x38,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xc8,
	0x030B, 0x02,
	0x030D, 0x08,
	0x030E, 0x01,
	0x030F, 0xF4,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x05,
	0x3C12, 0x06,
	0x3C13, 0x28,
	0x3F0C, 0x00,
	0x3F14, 0x00,
	0x3F80, 0x00,
	0x3F81, 0x14,
	0x3F8C, 0x02,
	0x3F8D, 0xbc,
	0x3FF8, 0x00,
	0x3FF9, 0x00,
	0x3FFE, 0x01,
	0x3FFF, 0x54,
	/*Integration Setting*/
	0x0202, 0x06,
	0x0203, 0xc2,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x01,
	0x3E3B, 0x00,
	0x4434, 0x00,
	0x4435, 0xF8,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x08,
	0x0850, 0x00,
	0x0851, 0x07,
	0x0852, 0x00,
	0x0853, 0x0E,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};

static kal_uint16 imx586_slim_video_setting[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xC0,
	/*Frame Length Lines Setting*/
	0x0340, 0x0E,
	0x0341, 0x9A,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0xE8,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x14,
	0x034B, 0x87,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x3140, 0x00,
	0x3246, 0x81,
	0x3247, 0x81,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0F,
	0x040D, 0xA0,
	0x040E, 0x08,
	0x040F, 0xD0,
	/*Output Size Setting*/
	0x034C, 0x0F,
	0x034D, 0xA0,
	0x034E, 0x08,
	0x034F, 0xD0,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB8,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0x1C,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x04,
	0x3C12, 0x03,
	0x3C13, 0x2D,
	0x3F0C, 0x01,
	0x3F14, 0x00,
	0x3F80, 0x01,
	0x3F81, 0x90,
	0x3F8C, 0x00,
	0x3F8D, 0x14,
	0x3FF8, 0x01,
	0x3FF9, 0x2A,
	0x3FFE, 0x00,
	0x3FFF, 0x6C,
	/*Integration Setting*/
	0x0202, 0x0E,
	0x0203, 0x6A,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0xF0,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x08,
	0x0850, 0x00,
	0x0851, 0x07,
	0x0852, 0x00,
	0x0853, 0x0E,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};

static kal_uint16 imx586_custom1_setting[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x15,
	0x0343, 0x00,
	/*Frame Length Lines Setting*/
	0x0340, 0x05,
	0x0341, 0x3A,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0xE8,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x14,
	0x034B, 0x87,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x0a,
	0x3140, 0x00,
	0x3246, 0x89,
	0x3247, 0x89,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x28,
	0x040A, 0x00,
	0x040B, 0x18,
	0x040C, 0x07,
	0x040D, 0x80,
	0x040E, 0x04,
	0x040F, 0x38,
	/*Output Size Setting*/
	0x034C, 0x07,
	0x034D, 0x80,
	0x034E, 0x04,
	0x034F, 0x38,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x68,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x58,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x0C,
	0x3C12, 0x05,
	0x3C13, 0x2C,
	0x3F0C, 0x00,
	0x3F14, 0x00,
	0x3F80, 0x02,
	0x3F81, 0x67,
	0x3F8C, 0x02,
	0x3F8D, 0x44,
	0x3FF8, 0x00,
	0x3FF9, 0x00,
	0x3FFE, 0x01,
	0x3FFF, 0x90,
	/*Integration Setting*/
	0x0202, 0x05,
	0x0203, 0x0A,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE1 Setting*/
	0x3E20, 0x01,
	0x3E37, 0x00,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x12,
	0x0852, 0x00,
	0x0853, 0x25,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};

static kal_uint16 imx586_custom4_setting[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x24,
	0x0343, 0xe0,
	/*Frame Length Lines Setting*/
	0x0340, 0x17,
	0x0341, 0xfe,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x17,
	0x034B, 0x6f,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0a,
	0x3140, 0x00,
	0x3246, 0x01,
	0x3247, 0x01,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x07,
	0x0409, 0xd0,
	0x040A, 0x05,
	0x040B, 0xdc,
	0x040C, 0x0f,
	0x040D, 0xa0,
	0x040E, 0x0b,
	0x040F, 0xb8,
	/*Output Size Setting*/
	0x034C, 0x0f,
	0x034D, 0xa0,
	0x034E, 0x0b,
	0x034F, 0xb8,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x68,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0xa0,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x01,
	0x3621, 0x01,
	0x3C11, 0x08,
	0x3C12, 0x08,
	0x3C13, 0x2a,
	0x3F0C, 0x00,
	0x3F14, 0x01,
	0x3F80, 0x02,
	0x3F81, 0x20,
	0x3F8C, 0x01,
	0x3F8D, 0x9a,
	0x3FF8, 0x00,
	0x3FF9, 0x00,
	0x3FFE, 0x02,
	0x3FFF, 0x0e,
	/*Integration Setting*/
	0x0202, 0x17,
	0x0203, 0xce,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0x00,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x12,
	0x0852, 0x00,
	0x0853, 0x25,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};

static kal_uint16 imx586_custom3_setting[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x24,
	0x0343, 0xe0,
	/*Frame Length Lines Setting*/
	0x0340, 0x17,
	0x0341, 0xfe,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x17,
	0x034B, 0x6f,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0a,
	0x3140, 0x00,
	0x3246, 0x01,
	0x3247, 0x01,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x1f,
	0x040D, 0x40,
	0x040E, 0x17,
	0x040F, 0x70,
	/*Output Size Setting*/
	0x034C, 0x1f,
	0x034D, 0x40,
	0x034E, 0x17,
	0x034F, 0x70,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x68,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0xa0,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x01,
	0x3621, 0x01,
	0x3C11, 0x08,
	0x3C12, 0x08,
	0x3C13, 0x2a,
	0x3F0C, 0x00,
	0x3F14, 0x01,
	0x3F80, 0x02,
	0x3F81, 0x20,
	0x3F8C, 0x01,
	0x3F8D, 0x9a,
	0x3FF8, 0x00,
	0x3FF9, 0x00,
	0x3FFE, 0x02,
	0x3FFF, 0x0e,
	/*Integration Setting*/
	0x0202, 0x17,
	0x0203, 0xce,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0xf0,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x12,
	0x0852, 0x00,
	0x0853, 0x25,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};

static kal_uint16 imx586_custom5_setting[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x15,
	0x0343, 0x00,
	/*Frame Length Lines Setting*/
	0x0340, 0x0A,
	0x0341, 0x76,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x17,
	0x034B, 0x6F,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x08,
	0x3140, 0x00,
	0x3246, 0x89,
	0x3247, 0x89,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x02,
	0x0409, 0xA8,
	0x040A, 0x01,
	0x040B, 0xFC,
	0x040C, 0x02,
	0x040D, 0x80,
	0x040E, 0x01,
	0x040F, 0xE0,
	/*Output Size Setting*/
	0x034C, 0x02,
	0x034D, 0x80,
	0x034E, 0x01,
	0x034F, 0xE0,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x68,
	0x030B, 0x02,
	0x030D, 0x0C,
	0x030E, 0x04,
	0x030F, 0x2A,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x00,
	0x3621, 0x00,
	0x3C11, 0x0C,
	0x3C12, 0x05,
	0x3C13, 0x2C,
	0x3F0C, 0x00,
	0x3F14, 0x00,
	0x3F80, 0x02,
	0x3F81, 0x67,
	0x3F8C, 0x02,
	0x3F8D, 0x44,
	0x3FF8, 0x00,
	0x3FF9, 0x00,
	0x3FFE, 0x01,
	0x3FFF, 0x90,
	/*Integration Setting*/
	0x0202, 0x0A,
	0x0203, 0x46,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x01,
	0x3E3B, 0x00,
	0x4434, 0x00,
	0x4435, 0xF8,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x0C,
	0x0850, 0x00,
	0x0851, 0x0A,
	0x0852, 0x00,
	0x0853, 0x14,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};

static kal_uint16 imx586_custom6_setting[] = {
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x24,
	0x0343, 0xE0,
	/*Frame Length Lines Setting*/
	0x0340, 0x1D,
	0x0341, 0xCB,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0xE8,
	0x0348, 0x1F,
	0x0349, 0x3F,
	0x034A, 0x14,
	0x034B, 0x87,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x3140, 0x00,
	0x3246, 0x01,
	0x3247, 0x01,
	0x3F15, 0x00,
	/*Digital Crop & Scaling*/
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0xA0,
	0x040A, 0x00,
	0x040B, 0x60,
	0x040C, 0x1E,
	0x040D, 0x00,
	0x040E, 0x10,
	0x040F, 0xE0,
	/*Output Size Setting*/
	0x034C, 0x1E,
	0x034D, 0x00,
	0x034E, 0x10,
	0x034F, 0xE0,
	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x68,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0xA0,
	0x0310, 0x01,
	/*Other Setting*/
	0x3620, 0x01,
	0x3621, 0x01,
	0x3C11, 0x08,
	0x3C12, 0x08,
	0x3C13, 0x2A,
	0x3F0C, 0x00,
	0x3F14, 0x01,
	0x3F80, 0x02,
	0x3F81, 0x20,
	0x3F8C, 0x01,
	0x3F8D, 0x9A,
	0x3FF8, 0x00,
	0x3FF9, 0x00,
	0x3FFE, 0x02,
	0x3FFF, 0x0E,
	/*Integration Setting*/
	0x0202, 0x1D,
	0x0203, 0x9B,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3FE0, 0x01,
	0x3FE1, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x70,
	0x0216, 0x00,
	0x0217, 0x70,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,
	0x3FE2, 0x00,
	0x3FE3, 0x70,
	0x3FE4, 0x01,
	0x3FE5, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3E20, 0x02,
	0x3E3B, 0x01,
	0x4434, 0x01,
	0x4435, 0xE0,
	/*224 UI*/
	0x0808, 0x02,
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x12,
	0x0852, 0x00,
	0x0853, 0x25,
	0x0854, 0x00,
	0x0855, 0x14,
	0x0858, 0x00,
	0x0859, 0x1F,

};
static void sensor_init(struct subdrv_ctx *ctx)
{
	pr_debug("[%s] start\n", __func__);
	#if USE_BURST_MODE
	imx586_table_write_cmos_sensor(ctx, imx586_init_setting,
		sizeof(imx586_init_setting)/sizeof(kal_uint16));
	#else
	/*External Clock Setting*/
	write_cmos_sensor_8(ctx, 0x0136, 0x18);
	write_cmos_sensor_8(ctx, 0x0137, 0x00);
	/*Register version*/
	write_cmos_sensor_8(ctx, 0x3C7E, 0x04);
	write_cmos_sensor_8(ctx, 0x3C7F, 0x08);
	/*Signaling mode setting*/
	write_cmos_sensor_8(ctx, 0x0111, 0x03);
	/*Global Setting*/
	write_cmos_sensor_8(ctx, 0x380C, 0x00);
	write_cmos_sensor_8(ctx, 0x3C00, 0x10);
	write_cmos_sensor_8(ctx, 0x3C01, 0x10);
	write_cmos_sensor_8(ctx, 0x3C02, 0x10);
	write_cmos_sensor_8(ctx, 0x3C03, 0x10);
	write_cmos_sensor_8(ctx, 0x3C04, 0x10);
	write_cmos_sensor_8(ctx, 0x3C05, 0x01);
	write_cmos_sensor_8(ctx, 0x3C06, 0x00);
	write_cmos_sensor_8(ctx, 0x3C07, 0x00);
	write_cmos_sensor_8(ctx, 0x3C08, 0x03);
	write_cmos_sensor_8(ctx, 0x3C09, 0xFF);
	write_cmos_sensor_8(ctx, 0x3C0A, 0x01);
	write_cmos_sensor_8(ctx, 0x3C0B, 0x00);
	write_cmos_sensor_8(ctx, 0x3C0C, 0x00);
	write_cmos_sensor_8(ctx, 0x3C0D, 0x03);
	write_cmos_sensor_8(ctx, 0x3C0E, 0xFF);
	write_cmos_sensor_8(ctx, 0x3C0F, 0x20);
	write_cmos_sensor_8(ctx, 0x3F88, 0x00);
	write_cmos_sensor_8(ctx, 0x3F8E, 0x00);
	write_cmos_sensor_8(ctx, 0x5282, 0x01);
	write_cmos_sensor_8(ctx, 0x9004, 0x14);
	write_cmos_sensor_8(ctx, 0x9200, 0xF4);
	write_cmos_sensor_8(ctx, 0x9201, 0xA7);
	write_cmos_sensor_8(ctx, 0x9202, 0xF4);
	write_cmos_sensor_8(ctx, 0x9203, 0xAA);
	write_cmos_sensor_8(ctx, 0x9204, 0xF4);
	write_cmos_sensor_8(ctx, 0x9205, 0xAD);
	write_cmos_sensor_8(ctx, 0x9206, 0xF4);
	write_cmos_sensor_8(ctx, 0x9207, 0xB0);
	write_cmos_sensor_8(ctx, 0x9208, 0xF4);
	write_cmos_sensor_8(ctx, 0x9209, 0xB3);
	write_cmos_sensor_8(ctx, 0x920A, 0xB7);
	write_cmos_sensor_8(ctx, 0x920B, 0x34);
	write_cmos_sensor_8(ctx, 0x920C, 0xB7);
	write_cmos_sensor_8(ctx, 0x920D, 0x36);
	write_cmos_sensor_8(ctx, 0x920E, 0xB7);
	write_cmos_sensor_8(ctx, 0x920F, 0x37);
	write_cmos_sensor_8(ctx, 0x9210, 0xB7);
	write_cmos_sensor_8(ctx, 0x9211, 0x38);
	write_cmos_sensor_8(ctx, 0x9212, 0xB7);
	write_cmos_sensor_8(ctx, 0x9213, 0x39);
	write_cmos_sensor_8(ctx, 0x9214, 0xB7);
	write_cmos_sensor_8(ctx, 0x9215, 0x3A);
	write_cmos_sensor_8(ctx, 0x9216, 0xB7);
	write_cmos_sensor_8(ctx, 0x9217, 0x3C);
	write_cmos_sensor_8(ctx, 0x9218, 0xB7);
	write_cmos_sensor_8(ctx, 0x9219, 0x3D);
	write_cmos_sensor_8(ctx, 0x921A, 0xB7);
	write_cmos_sensor_8(ctx, 0x921B, 0x3E);
	write_cmos_sensor_8(ctx, 0x921C, 0xB7);
	write_cmos_sensor_8(ctx, 0x921D, 0x3F);
	write_cmos_sensor_8(ctx, 0x921E, 0x77);
	write_cmos_sensor_8(ctx, 0x921F, 0x77);
	write_cmos_sensor_8(ctx, 0x9222, 0xC4);
	write_cmos_sensor_8(ctx, 0x9223, 0x4B);
	write_cmos_sensor_8(ctx, 0x9224, 0xC4);
	write_cmos_sensor_8(ctx, 0x9225, 0x4C);
	write_cmos_sensor_8(ctx, 0x9226, 0xC4);
	write_cmos_sensor_8(ctx, 0x9227, 0x4D);
	write_cmos_sensor_8(ctx, 0x9810, 0x14);
	write_cmos_sensor_8(ctx, 0x9814, 0x14);
	write_cmos_sensor_8(ctx, 0x99B2, 0x20);
	write_cmos_sensor_8(ctx, 0x99B3, 0x0F);
	write_cmos_sensor_8(ctx, 0x99B4, 0x0F);
	write_cmos_sensor_8(ctx, 0x99B5, 0x0F);
	write_cmos_sensor_8(ctx, 0x99B6, 0x0F);
	write_cmos_sensor_8(ctx, 0x99E4, 0x0F);
	write_cmos_sensor_8(ctx, 0x99E5, 0x0F);
	write_cmos_sensor_8(ctx, 0x99E6, 0x0F);
	write_cmos_sensor_8(ctx, 0x99E7, 0x0F);
	write_cmos_sensor_8(ctx, 0x99E8, 0x0F);
	write_cmos_sensor_8(ctx, 0x99E9, 0x0F);
	write_cmos_sensor_8(ctx, 0x99EA, 0x0F);
	write_cmos_sensor_8(ctx, 0x99EB, 0x0F);
	write_cmos_sensor_8(ctx, 0x99EC, 0x0F);
	write_cmos_sensor_8(ctx, 0x99ED, 0x0F);
	write_cmos_sensor_8(ctx, 0xA569, 0x06);
	write_cmos_sensor_8(ctx, 0xA679, 0x20);
	write_cmos_sensor_8(ctx, 0xC020, 0x01);
	write_cmos_sensor_8(ctx, 0xC61D, 0x00);
	write_cmos_sensor_8(ctx, 0xC625, 0x00);
	write_cmos_sensor_8(ctx, 0xC638, 0x03);
	write_cmos_sensor_8(ctx, 0xC63B, 0x01);
	write_cmos_sensor_8(ctx, 0xE286, 0x31);
	write_cmos_sensor_8(ctx, 0xE2A6, 0x32);
	write_cmos_sensor_8(ctx, 0xE2C6, 0x33);
	#endif
	/*enable temperature sensor, TEMP_SEN_CTL:*/
	write_cmos_sensor_8(ctx, 0x0138, 0x01);
	/* set MIPI auto ctrl */
	write_cmos_sensor(ctx, 0x0808, 0x00);

	set_mirror_flip(ctx, ctx->mirror);
	pr_debug("[%s] End\n", __func__);
}	/*	  sensor_init  */

static void preview_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s +\n", __func__);

	#if USE_BURST_MODE
	imx586_table_write_cmos_sensor(ctx, imx586_preview_setting,
		sizeof(imx586_preview_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(ctx, 0x0112, 0x0A);
	write_cmos_sensor_8(ctx, 0x0113, 0x0A);
	write_cmos_sensor_8(ctx, 0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(ctx, 0x0342, 0x1E);
	write_cmos_sensor_8(ctx, 0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(ctx, 0x0340, 0x0B);
	write_cmos_sensor_8(ctx, 0x0341, 0xFC);
	/*ROI Setting*/
	write_cmos_sensor_8(ctx, 0x0344, 0x00);
	write_cmos_sensor_8(ctx, 0x0345, 0x00);
	write_cmos_sensor_8(ctx, 0x0346, 0x00);
	write_cmos_sensor_8(ctx, 0x0347, 0x00);
	write_cmos_sensor_8(ctx, 0x0348, 0x1F);
	write_cmos_sensor_8(ctx, 0x0349, 0x3F);
	write_cmos_sensor_8(ctx, 0x034A, 0x17);
	write_cmos_sensor_8(ctx, 0x034B, 0x6F);
	/*Mode Setting*/
	write_cmos_sensor_8(ctx, 0x0220, 0x62);
	write_cmos_sensor_8(ctx, 0x0222, 0x01);
	write_cmos_sensor_8(ctx, 0x0900, 0x01);
	write_cmos_sensor_8(ctx, 0x0901, 0x22);
	write_cmos_sensor_8(ctx, 0x0902, 0x08);
	write_cmos_sensor_8(ctx, 0x3140, 0x00);
	write_cmos_sensor_8(ctx, 0x3246, 0x81);
	write_cmos_sensor_8(ctx, 0x3247, 0x81);
	write_cmos_sensor_8(ctx, 0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(ctx, 0x0401, 0x00);
	write_cmos_sensor_8(ctx, 0x0404, 0x00);
	write_cmos_sensor_8(ctx, 0x0405, 0x10);
	write_cmos_sensor_8(ctx, 0x0408, 0x00);
	write_cmos_sensor_8(ctx, 0x0409, 0x00);
	write_cmos_sensor_8(ctx, 0x040A, 0x00);
	write_cmos_sensor_8(ctx, 0x040B, 0x00);
	write_cmos_sensor_8(ctx, 0x040C, 0x0F);
	write_cmos_sensor_8(ctx, 0x040D, 0xA0);
	write_cmos_sensor_8(ctx, 0x040E, 0x0B);
	write_cmos_sensor_8(ctx, 0x040F, 0xB8);
	/*Output Size Setting*/
	write_cmos_sensor_8(ctx, 0x034C, 0x0F);
	write_cmos_sensor_8(ctx, 0x034D, 0xA0);
	write_cmos_sensor_8(ctx, 0x034E, 0x0B);
	write_cmos_sensor_8(ctx, 0x034F, 0xB8);
	/*Clock Setting*/
	write_cmos_sensor_8(ctx, 0x0301, 0x05);
	write_cmos_sensor_8(ctx, 0x0303, 0x02);
	write_cmos_sensor_8(ctx, 0x0305, 0x04);
	write_cmos_sensor_8(ctx, 0x0306, 0x01);
	write_cmos_sensor_8(ctx, 0x0307, 0x2E);
	write_cmos_sensor_8(ctx, 0x030B, 0x01);
	write_cmos_sensor_8(ctx, 0x030D, 0x0C);
	write_cmos_sensor_8(ctx, 0x030E, 0x02);
	write_cmos_sensor_8(ctx, 0x030F, 0xC6);
	write_cmos_sensor_8(ctx, 0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(ctx, 0x3620, 0x00);
	write_cmos_sensor_8(ctx, 0x3621, 0x00);
	write_cmos_sensor_8(ctx, 0x3C11, 0x04);
	write_cmos_sensor_8(ctx, 0x3C12, 0x03);
	write_cmos_sensor_8(ctx, 0x3C13, 0x2D);
	write_cmos_sensor_8(ctx, 0x3F0C, 0x01);
	write_cmos_sensor_8(ctx, 0x3F14, 0x00);
	write_cmos_sensor_8(ctx, 0x3F80, 0x01);
	write_cmos_sensor_8(ctx, 0x3F81, 0x90);
	write_cmos_sensor_8(ctx, 0x3F8C, 0x00);
	write_cmos_sensor_8(ctx, 0x3F8D, 0x14);
	write_cmos_sensor_8(ctx, 0x3FF8, 0x01);
	write_cmos_sensor_8(ctx, 0x3FF9, 0x2A);
	write_cmos_sensor_8(ctx, 0x3FFE, 0x00);
	write_cmos_sensor_8(ctx, 0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(ctx, 0x0202, 0x0B);
	write_cmos_sensor_8(ctx, 0x0203, 0xCC);
	write_cmos_sensor_8(ctx, 0x0224, 0x01);
	write_cmos_sensor_8(ctx, 0x0225, 0xF4);
	write_cmos_sensor_8(ctx, 0x3FE0, 0x01);
	write_cmos_sensor_8(ctx, 0x3FE1, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(ctx, 0x0204, 0x00);
	write_cmos_sensor_8(ctx, 0x0205, 0x70);
	write_cmos_sensor_8(ctx, 0x0216, 0x00);
	write_cmos_sensor_8(ctx, 0x0217, 0x70);
	write_cmos_sensor_8(ctx, 0x0218, 0x01);
	write_cmos_sensor_8(ctx, 0x0219, 0x00);
	write_cmos_sensor_8(ctx, 0x020E, 0x01);
	write_cmos_sensor_8(ctx, 0x020F, 0x00);
	write_cmos_sensor_8(ctx, 0x0210, 0x01);
	write_cmos_sensor_8(ctx, 0x0211, 0x00);
	write_cmos_sensor_8(ctx, 0x0212, 0x01);
	write_cmos_sensor_8(ctx, 0x0213, 0x00);
	write_cmos_sensor_8(ctx, 0x0214, 0x01);
	write_cmos_sensor_8(ctx, 0x0215, 0x00);
	write_cmos_sensor_8(ctx, 0x3FE2, 0x00);
	write_cmos_sensor_8(ctx, 0x3FE3, 0x70);
	write_cmos_sensor_8(ctx, 0x3FE4, 0x01);
	write_cmos_sensor_8(ctx, 0x3FE5, 0x00);
	/*PDAF TYPE1 Setting*/
	write_cmos_sensor_8(ctx, 0x3E20, 0x02);
	write_cmos_sensor_8(ctx, 0x3E37, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(ctx, 0x3E20, 0x02);
	write_cmos_sensor_8(ctx, 0x3E3B, 0x01);
	write_cmos_sensor_8(ctx, 0x4434, 0x01);
	write_cmos_sensor_8(ctx, 0x4435, 0xF0);
	#endif
	pr_debug("%s -\n", __func__);
} /* preview_setting */


/*full size 30fps*/
static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	int _length = 0;

	_length = sizeof(imx586_capture_setting) / sizeof(kal_uint16);
	DEBUG_LOG(ctx, "%s fps E! currefps:%d\n", __func__, currefps);
	/*************MIPI output setting************/
	if (!_is_seamless)
		imx586_table_write_cmos_sensor(ctx, imx586_capture_setting,
		_length);
	else {
		LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
			__func__, _is_seamless, _size_to_write);
		if (_size_to_write + _length > _I2C_BUF_SIZE) {
			LOG_INF("_too much i2c data for fast siwtch %d\n",
				_size_to_write + _length);
			return;
		}
		memcpy((void *) (_i2c_data + _size_to_write),
			imx586_capture_setting,
			sizeof(imx586_capture_setting));
		_size_to_write += _length;
	}
	LOG_INF("%s fpsX\n", __func__);
}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	pr_debug("%s E! currefps:%d\n", __func__, currefps);

	#if USE_BURST_MODE
	imx586_table_write_cmos_sensor(ctx, imx586_normal_video_setting,
		sizeof(imx586_normal_video_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(ctx, 0x0112, 0x0A);
	write_cmos_sensor_8(ctx, 0x0113, 0x0A);
	write_cmos_sensor_8(ctx, 0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(ctx, 0x0342, 0x1E);
	write_cmos_sensor_8(ctx, 0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(ctx, 0x0340, 0x0E);
	write_cmos_sensor_8(ctx, 0x0341, 0x9A);
	/*ROI Setting*/
	write_cmos_sensor_8(ctx, 0x0344, 0x00);
	write_cmos_sensor_8(ctx, 0x0345, 0x00);
	write_cmos_sensor_8(ctx, 0x0346, 0x01);
	write_cmos_sensor_8(ctx, 0x0347, 0x90);
	write_cmos_sensor_8(ctx, 0x0348, 0x1F);
	write_cmos_sensor_8(ctx, 0x0349, 0x3F);
	write_cmos_sensor_8(ctx, 0x034A, 0x15);
	write_cmos_sensor_8(ctx, 0x034B, 0xDF);
	/*Mode Setting*/
	write_cmos_sensor_8(ctx, 0x0220, 0x62);
	write_cmos_sensor_8(ctx, 0x0222, 0x01);
	write_cmos_sensor_8(ctx, 0x0900, 0x01);
	write_cmos_sensor_8(ctx, 0x0901, 0x22);
	write_cmos_sensor_8(ctx, 0x0902, 0x08);
	write_cmos_sensor_8(ctx, 0x3140, 0x00);
	write_cmos_sensor_8(ctx, 0x3246, 0x81);
	write_cmos_sensor_8(ctx, 0x3247, 0x81);
	write_cmos_sensor_8(ctx, 0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(ctx, 0x0401, 0x00);
	write_cmos_sensor_8(ctx, 0x0404, 0x00);
	write_cmos_sensor_8(ctx, 0x0405, 0x10);
	write_cmos_sensor_8(ctx, 0x0408, 0x00);
	write_cmos_sensor_8(ctx, 0x0409, 0x00);
	write_cmos_sensor_8(ctx, 0x040A, 0x00);
	write_cmos_sensor_8(ctx, 0x040B, 0x00);
	write_cmos_sensor_8(ctx, 0x040C, 0x0F);
	write_cmos_sensor_8(ctx, 0x040D, 0xA0);
	write_cmos_sensor_8(ctx, 0x040E, 0x0A);
	write_cmos_sensor_8(ctx, 0x040F, 0x28);
	/*Output Size Setting*/
	write_cmos_sensor_8(ctx, 0x034C, 0x0F);
	write_cmos_sensor_8(ctx, 0x034D, 0xA0);
	write_cmos_sensor_8(ctx, 0x034E, 0x0A);
	write_cmos_sensor_8(ctx, 0x034F, 0x28);
	/*Clock Setting*/
	write_cmos_sensor_8(ctx, 0x0301, 0x05);
	write_cmos_sensor_8(ctx, 0x0303, 0x02);
	write_cmos_sensor_8(ctx, 0x0305, 0x04);
	write_cmos_sensor_8(ctx, 0x0306, 0x00);
	write_cmos_sensor_8(ctx, 0x0307, 0xB8);
	write_cmos_sensor_8(ctx, 0x030B, 0x02);
	write_cmos_sensor_8(ctx, 0x030D, 0x04);
	write_cmos_sensor_8(ctx, 0x030E, 0x01);
	write_cmos_sensor_8(ctx, 0x030F, 0x1C);
	write_cmos_sensor_8(ctx, 0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(ctx, 0x3620, 0x00);
	write_cmos_sensor_8(ctx, 0x3621, 0x00);
	write_cmos_sensor_8(ctx, 0x3C11, 0x04);
	write_cmos_sensor_8(ctx, 0x3C12, 0x03);
	write_cmos_sensor_8(ctx, 0x3C13, 0x2D);
	write_cmos_sensor_8(ctx, 0x3F0C, 0x01);
	write_cmos_sensor_8(ctx, 0x3F14, 0x00);
	write_cmos_sensor_8(ctx, 0x3F80, 0x01);
	write_cmos_sensor_8(ctx, 0x3F81, 0x90);
	write_cmos_sensor_8(ctx, 0x3F8C, 0x00);
	write_cmos_sensor_8(ctx, 0x3F8D, 0x14);
	write_cmos_sensor_8(ctx, 0x3FF8, 0x01);
	write_cmos_sensor_8(ctx, 0x3FF9, 0x2A);
	write_cmos_sensor_8(ctx, 0x3FFE, 0x00);
	write_cmos_sensor_8(ctx, 0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(ctx, 0x0202, 0x0E);
	write_cmos_sensor_8(ctx, 0x0203, 0x6A);
	write_cmos_sensor_8(ctx, 0x0224, 0x01);
	write_cmos_sensor_8(ctx, 0x0225, 0xF4);
	write_cmos_sensor_8(ctx, 0x3FE0, 0x01);
	write_cmos_sensor_8(ctx, 0x3FE1, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(ctx, 0x0204, 0x00);
	write_cmos_sensor_8(ctx, 0x0205, 0x70);
	write_cmos_sensor_8(ctx, 0x0216, 0x00);
	write_cmos_sensor_8(ctx, 0x0217, 0x70);
	write_cmos_sensor_8(ctx, 0x0218, 0x01);
	write_cmos_sensor_8(ctx, 0x0219, 0x00);
	write_cmos_sensor_8(ctx, 0x020E, 0x01);
	write_cmos_sensor_8(ctx, 0x020F, 0x00);
	write_cmos_sensor_8(ctx, 0x0210, 0x01);
	write_cmos_sensor_8(ctx, 0x0211, 0x00);
	write_cmos_sensor_8(ctx, 0x0212, 0x01);
	write_cmos_sensor_8(ctx, 0x0213, 0x00);
	write_cmos_sensor_8(ctx, 0x0214, 0x01);
	write_cmos_sensor_8(ctx, 0x0215, 0x00);
	write_cmos_sensor_8(ctx, 0x3FE2, 0x00);
	write_cmos_sensor_8(ctx, 0x3FE3, 0x70);
	write_cmos_sensor_8(ctx, 0x3FE4, 0x01);
	write_cmos_sensor_8(ctx, 0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(ctx, 0x3E20, 0x02);
	write_cmos_sensor_8(ctx, 0x3E3B, 0x01);
	write_cmos_sensor_8(ctx, 0x4434, 0x01);
	write_cmos_sensor_8(ctx, 0x4435, 0xF0);
	#endif
	pr_debug("X\n");
}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s E! currefps 120\n", __func__);

	#if USE_BURST_MODE
	imx586_table_write_cmos_sensor(ctx, imx586_hs_video_setting,
		sizeof(imx586_hs_video_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(ctx, 0x0112, 0x0A);
	write_cmos_sensor_8(ctx, 0x0113, 0x0A);
	write_cmos_sensor_8(ctx, 0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(ctx, 0x0342, 0x15);
	write_cmos_sensor_8(ctx, 0x0343, 0x00);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(ctx, 0x0340, 0x04);
	write_cmos_sensor_8(ctx, 0x0341, 0xA6);
	/*ROI Setting*/
	write_cmos_sensor_8(ctx, 0x0344, 0x00);
	write_cmos_sensor_8(ctx, 0x0345, 0x00);
	write_cmos_sensor_8(ctx, 0x0346, 0x02);
	write_cmos_sensor_8(ctx, 0x0347, 0xE8);
	write_cmos_sensor_8(ctx, 0x0348, 0x1F);
	write_cmos_sensor_8(ctx, 0x0349, 0x3F);
	write_cmos_sensor_8(ctx, 0x034A, 0x14);
	write_cmos_sensor_8(ctx, 0x034B, 0x87);
	/*Mode Setting*/
	write_cmos_sensor_8(ctx, 0x0220, 0x62);
	write_cmos_sensor_8(ctx, 0x0222, 0x01);
	write_cmos_sensor_8(ctx, 0x0900, 0x01);
	write_cmos_sensor_8(ctx, 0x0901, 0x44);
	write_cmos_sensor_8(ctx, 0x0902, 0x08);
	write_cmos_sensor_8(ctx, 0x3140, 0x00);
	write_cmos_sensor_8(ctx, 0x3246, 0x89);
	write_cmos_sensor_8(ctx, 0x3247, 0x89);
	write_cmos_sensor_8(ctx, 0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(ctx, 0x0401, 0x00);
	write_cmos_sensor_8(ctx, 0x0404, 0x00);
	write_cmos_sensor_8(ctx, 0x0405, 0x10);
	write_cmos_sensor_8(ctx, 0x0408, 0x00);
	write_cmos_sensor_8(ctx, 0x0409, 0x00);
	write_cmos_sensor_8(ctx, 0x040A, 0x00);
	write_cmos_sensor_8(ctx, 0x040B, 0x00);
	write_cmos_sensor_8(ctx, 0x040C, 0x07);
	write_cmos_sensor_8(ctx, 0x040D, 0xD0);
	write_cmos_sensor_8(ctx, 0x040E, 0x04);
	write_cmos_sensor_8(ctx, 0x040F, 0x68);
	/*Output Size Setting*/
	write_cmos_sensor_8(ctx, 0x034C, 0x07);
	write_cmos_sensor_8(ctx, 0x034D, 0xD0);
	write_cmos_sensor_8(ctx, 0x034E, 0x04);
	write_cmos_sensor_8(ctx, 0x034F, 0x68);
	/*Clock Setting*/
	write_cmos_sensor_8(ctx, 0x0301, 0x05);
	write_cmos_sensor_8(ctx, 0x0303, 0x04);
	write_cmos_sensor_8(ctx, 0x0305, 0x04);
	write_cmos_sensor_8(ctx, 0x0306, 0x01);
	write_cmos_sensor_8(ctx, 0x0307, 0x40);
	write_cmos_sensor_8(ctx, 0x030B, 0x04);
	write_cmos_sensor_8(ctx, 0x030D, 0x0C);
	write_cmos_sensor_8(ctx, 0x030E, 0x03);
	write_cmos_sensor_8(ctx, 0x030F, 0xF0);
	write_cmos_sensor_8(ctx, 0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(ctx, 0x3620, 0x00);
	write_cmos_sensor_8(ctx, 0x3621, 0x00);
	write_cmos_sensor_8(ctx, 0x3C11, 0x0C);
	write_cmos_sensor_8(ctx, 0x3C12, 0x05);
	write_cmos_sensor_8(ctx, 0x3C13, 0x2C);
	write_cmos_sensor_8(ctx, 0x3F0C, 0x00);
	write_cmos_sensor_8(ctx, 0x3F14, 0x00);
	write_cmos_sensor_8(ctx, 0x3F80, 0x02);
	write_cmos_sensor_8(ctx, 0x3F81, 0x67);
	write_cmos_sensor_8(ctx, 0x3F8C, 0x02);
	write_cmos_sensor_8(ctx, 0x3F8D, 0x44);
	write_cmos_sensor_8(ctx, 0x3FF8, 0x00);
	write_cmos_sensor_8(ctx, 0x3FF9, 0x00);
	write_cmos_sensor_8(ctx, 0x3FFE, 0x01);
	write_cmos_sensor_8(ctx, 0x3FFF, 0x90);
	/*Integration Setting*/
	write_cmos_sensor_8(ctx, 0x0202, 0x04);
	write_cmos_sensor_8(ctx, 0x0203, 0x76);
	write_cmos_sensor_8(ctx, 0x0224, 0x01);
	write_cmos_sensor_8(ctx, 0x0225, 0xF4);
	write_cmos_sensor_8(ctx, 0x3FE0, 0x01);
	write_cmos_sensor_8(ctx, 0x3FE1, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(ctx, 0x0204, 0x00);
	write_cmos_sensor_8(ctx, 0x0205, 0x70);
	write_cmos_sensor_8(ctx, 0x0216, 0x00);
	write_cmos_sensor_8(ctx, 0x0217, 0x70);
	write_cmos_sensor_8(ctx, 0x0218, 0x01);
	write_cmos_sensor_8(ctx, 0x0219, 0x00);
	write_cmos_sensor_8(ctx, 0x020E, 0x01);
	write_cmos_sensor_8(ctx, 0x020F, 0x00);
	write_cmos_sensor_8(ctx, 0x0210, 0x01);
	write_cmos_sensor_8(ctx, 0x0211, 0x00);
	write_cmos_sensor_8(ctx, 0x0212, 0x01);
	write_cmos_sensor_8(ctx, 0x0213, 0x00);
	write_cmos_sensor_8(ctx, 0x0214, 0x01);
	write_cmos_sensor_8(ctx, 0x0215, 0x00);
	write_cmos_sensor_8(ctx, 0x3FE2, 0x00);
	write_cmos_sensor_8(ctx, 0x3FE3, 0x70);
	write_cmos_sensor_8(ctx, 0x3FE4, 0x01);
	write_cmos_sensor_8(ctx, 0x3FE5, 0x00);
	/*PDAF TYPE1 Setting*/
	write_cmos_sensor_8(ctx, 0x3E20, 0x01);
	write_cmos_sensor_8(ctx, 0x3E37, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(ctx, 0x3E20, 0x01);
	write_cmos_sensor_8(ctx, 0x3E3B, 0x00);
	write_cmos_sensor_8(ctx, 0x4434, 0x00);
	write_cmos_sensor_8(ctx, 0x4435, 0xF8);
	#endif
	pr_debug("X\n");
}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s E! 4000*2256@30fps\n", __func__);

	#if USE_BURST_MODE
	imx586_table_write_cmos_sensor(ctx, imx586_slim_video_setting,
		sizeof(imx586_slim_video_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(ctx, 0x0112, 0x0A);
	write_cmos_sensor_8(ctx, 0x0113, 0x0A);
	write_cmos_sensor_8(ctx, 0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(ctx, 0x0342, 0x1E);
	write_cmos_sensor_8(ctx, 0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(ctx, 0x0340, 0x0E);
	write_cmos_sensor_8(ctx, 0x0341, 0x9A);
	/*ROI Setting*/
	write_cmos_sensor_8(ctx, 0x0344, 0x00);
	write_cmos_sensor_8(ctx, 0x0345, 0x00);
	write_cmos_sensor_8(ctx, 0x0346, 0x02);
	write_cmos_sensor_8(ctx, 0x0347, 0xE8);
	write_cmos_sensor_8(ctx, 0x0348, 0x1F);
	write_cmos_sensor_8(ctx, 0x0349, 0x3F);
	write_cmos_sensor_8(ctx, 0x034A, 0x14);
	write_cmos_sensor_8(ctx, 0x034B, 0x87);
	/*Mode Setting*/
	write_cmos_sensor_8(ctx, 0x0220, 0x62);
	write_cmos_sensor_8(ctx, 0x0222, 0x01);
	write_cmos_sensor_8(ctx, 0x0900, 0x01);
	write_cmos_sensor_8(ctx, 0x0901, 0x22);
	write_cmos_sensor_8(ctx, 0x0902, 0x08);
	write_cmos_sensor_8(ctx, 0x3140, 0x00);
	write_cmos_sensor_8(ctx, 0x3246, 0x81);
	write_cmos_sensor_8(ctx, 0x3247, 0x81);
	write_cmos_sensor_8(ctx, 0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(ctx, 0x0401, 0x00);
	write_cmos_sensor_8(ctx, 0x0404, 0x00);
	write_cmos_sensor_8(ctx, 0x0405, 0x10);
	write_cmos_sensor_8(ctx, 0x0408, 0x00);
	write_cmos_sensor_8(ctx, 0x0409, 0x00);
	write_cmos_sensor_8(ctx, 0x040A, 0x00);
	write_cmos_sensor_8(ctx, 0x040B, 0x00);
	write_cmos_sensor_8(ctx, 0x040C, 0x0F);
	write_cmos_sensor_8(ctx, 0x040D, 0xA0);
	write_cmos_sensor_8(ctx, 0x040E, 0x08);
	write_cmos_sensor_8(ctx, 0x040F, 0xD0);
	/*Output Size Setting*/
	write_cmos_sensor_8(ctx, 0x034C, 0x0F);
	write_cmos_sensor_8(ctx, 0x034D, 0xA0);
	write_cmos_sensor_8(ctx, 0x034E, 0x08);
	write_cmos_sensor_8(ctx, 0x034F, 0xD0);
	/*Clock Setting*/
	write_cmos_sensor_8(ctx, 0x0301, 0x05);
	write_cmos_sensor_8(ctx, 0x0303, 0x02);
	write_cmos_sensor_8(ctx, 0x0305, 0x04);
	write_cmos_sensor_8(ctx, 0x0306, 0x00);
	write_cmos_sensor_8(ctx, 0x0307, 0xB8);
	write_cmos_sensor_8(ctx, 0x030B, 0x02);
	write_cmos_sensor_8(ctx, 0x030D, 0x04);
	write_cmos_sensor_8(ctx, 0x030E, 0x01);
	write_cmos_sensor_8(ctx, 0x030F, 0x1C);
	write_cmos_sensor_8(ctx, 0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(ctx, 0x3620, 0x00);
	write_cmos_sensor_8(ctx, 0x3621, 0x00);
	write_cmos_sensor_8(ctx, 0x3C11, 0x04);
	write_cmos_sensor_8(ctx, 0x3C12, 0x03);
	write_cmos_sensor_8(ctx, 0x3C13, 0x2D);
	write_cmos_sensor_8(ctx, 0x3F0C, 0x01);
	write_cmos_sensor_8(ctx, 0x3F14, 0x00);
	write_cmos_sensor_8(ctx, 0x3F80, 0x01);
	write_cmos_sensor_8(ctx, 0x3F81, 0x90);
	write_cmos_sensor_8(ctx, 0x3F8C, 0x00);
	write_cmos_sensor_8(ctx, 0x3F8D, 0x14);
	write_cmos_sensor_8(ctx, 0x3FF8, 0x01);
	write_cmos_sensor_8(ctx, 0x3FF9, 0x2A);
	write_cmos_sensor_8(ctx, 0x3FFE, 0x00);
	write_cmos_sensor_8(ctx, 0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(ctx, 0x0202, 0x0E);
	write_cmos_sensor_8(ctx, 0x0203, 0x6A);
	write_cmos_sensor_8(ctx, 0x0224, 0x01);
	write_cmos_sensor_8(ctx, 0x0225, 0xF4);
	write_cmos_sensor_8(ctx, 0x3FE0, 0x01);
	write_cmos_sensor_8(ctx, 0x3FE1, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(ctx, 0x0204, 0x00);
	write_cmos_sensor_8(ctx, 0x0205, 0x70);
	write_cmos_sensor_8(ctx, 0x0216, 0x00);
	write_cmos_sensor_8(ctx, 0x0217, 0x70);
	write_cmos_sensor_8(ctx, 0x0218, 0x01);
	write_cmos_sensor_8(ctx, 0x0219, 0x00);
	write_cmos_sensor_8(ctx, 0x020E, 0x01);
	write_cmos_sensor_8(ctx, 0x020F, 0x00);
	write_cmos_sensor_8(ctx, 0x0210, 0x01);
	write_cmos_sensor_8(ctx, 0x0211, 0x00);
	write_cmos_sensor_8(ctx, 0x0212, 0x01);
	write_cmos_sensor_8(ctx, 0x0213, 0x00);
	write_cmos_sensor_8(ctx, 0x0214, 0x01);
	write_cmos_sensor_8(ctx, 0x0215, 0x00);
	write_cmos_sensor_8(ctx, 0x3FE2, 0x00);
	write_cmos_sensor_8(ctx, 0x3FE3, 0x70);
	write_cmos_sensor_8(ctx, 0x3FE4, 0x01);
	write_cmos_sensor_8(ctx, 0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(ctx, 0x3E20, 0x02);
	write_cmos_sensor_8(ctx, 0x3E3B, 0x01);
	write_cmos_sensor_8(ctx, 0x4434, 0x01);
	write_cmos_sensor_8(ctx, 0x4435, 0xF0);
	#endif
	pr_debug("X\n");
}


/*full size 30fps*/
static void custom3_setting(struct subdrv_ctx *ctx)
{
	int _length = 0;

	_length = sizeof(imx586_custom3_setting) / sizeof(kal_uint16);

	LOG_INF("%s full size 30 fps E!\n", __func__);
	/*************MIPI output setting************/

	if (!_is_seamless)
		imx586_table_write_cmos_sensor(ctx, imx586_custom3_setting, _length);
	else {
		LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
			__func__, _is_seamless, _size_to_write);
		if (_size_to_write + _length > _I2C_BUF_SIZE) {
			LOG_INF("_too much i2c data for fast siwtch %d\n",
				_size_to_write + _length);
			return;
		}
		memcpy((void *) (_i2c_data + _size_to_write),
			imx586_custom3_setting,
			sizeof(imx586_custom3_setting));
		_size_to_write += _length;
	}
	if (otp_flag == OTP_QSC_NONE) {
		pr_info("OTP no QSC Data, close qsc register");
		if (!_is_seamless)
			write_cmos_sensor_8(ctx, 0x3621, 0x00);
		else {
			_i2c_data[_size_to_write++] = 0x3621;
			_i2c_data[_size_to_write++] = 0x00;
		}
	}



	LOG_INF("%s 30 fpsX\n", __func__);
}

static void custom4_setting(struct subdrv_ctx *ctx)
{
	int _length = 0;

	_length = sizeof(imx586_custom4_setting) /
		sizeof(kal_uint16);

	if (!_is_seamless)
		imx586_table_write_cmos_sensor(ctx, imx586_custom4_setting, _length);
	else {
		LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
			__func__, _is_seamless, _size_to_write);
		if (_size_to_write + _length > _I2C_BUF_SIZE) {
			LOG_INF("_too much i2c data for fast siwtch %d\n",
				_size_to_write + _length);
			return;
		}
		memcpy((void *) (_i2c_data + _size_to_write),
			imx586_custom4_setting,
			sizeof(imx586_custom4_setting));
		_size_to_write += _length;

	}
	if (otp_flag == OTP_QSC_NONE) {
		pr_info("OTP no QSC Data, close qsc register");
		if (!_is_seamless)
			write_cmos_sensor_8(ctx, 0x3621, 0x00);
		else {
			_i2c_data[_size_to_write++] = 0x3621;
			_i2c_data[_size_to_write++] = 0x00;
		}
	}

	LOG_INF("X\n");
}
static void custom6_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s 8K@24 fps E! currefps\n", __func__);

	imx586_table_write_cmos_sensor(ctx, imx586_custom6_setting,
		sizeof(imx586_custom6_setting)/sizeof(kal_uint16));

	if (otp_flag == OTP_QSC_NONE) {
		pr_info("OTP no QSC Data, close qsc register");
		write_cmos_sensor_8(ctx, 0x3621, 0x00);
	}
	LOG_INF("X\n");
}

static void custom1_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s fhd 240fps E! currefps\n", __func__);
	/*************MIPI output setting************/
	imx586_table_write_cmos_sensor(ctx, imx586_custom1_setting,
		sizeof(imx586_custom1_setting)/sizeof(kal_uint16));
	pr_debug("X");
}

static void custom2_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s 3840*2160@60fps E! currefps\n", __func__);
	/*************MIPI output setting************/

	imx586_table_write_cmos_sensor(ctx, imx586_normal_video_setting_4K60FPS,
		sizeof(imx586_normal_video_setting_4K60FPS)/sizeof(kal_uint16));

	pr_debug("X");
}

static void custom5_setting(struct subdrv_ctx *ctx)
{
	pr_debug("%s 640*480 120fps E! currefps\n", __func__);
	/*************MIPI output setting************/
	imx586_table_write_cmos_sensor(ctx, imx586_custom5_setting,
		sizeof(imx586_custom5_setting)/sizeof(kal_uint16));
	pr_debug("X");
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
	/*sensor have two i2c address 0x34 & 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8)
					| read_cmos_sensor_8(ctx, 0x0017));
			pr_debug(
				"read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
				read_cmos_sensor_8(ctx, 0x0016),
				read_cmos_sensor_8(ctx, 0x0017),
				read_cmos_sensor(ctx, 0x0000));
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			pr_debug("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
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

	pr_debug("%s +\n", __func__);
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8)
					| read_cmos_sensor_8(ctx, 0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
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
	ctx->test_pattern = 0;
	ctx->current_fps = imgsensor_info.pre.max_framerate;
	pr_debug("%s -\n", __func__);

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
	pr_debug("E\n");
	/* No Need to implement this function */
	streaming_control(ctx, KAL_FALSE);
	qsc_flag = 0;
	_is_seamless = false;
	_size_to_write = 0;
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
static kal_uint32 preview(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);

	return ERROR_NONE;
} /* preview */
static kal_uint32 custom5_15(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

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
	pr_debug("E\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		DEBUG_LOG(ctx,
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			ctx->current_fps,
			imgsensor_info.cap.max_framerate / 10);

	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	capture_setting(ctx, ctx->current_fps);
	set_mirror_flip(ctx, ctx->mirror);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}

	return ERROR_NONE;
}	/* capture(ctx) */
static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	normal_video_setting(ctx, ctx->current_fps);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	/*ctx->video_mode = KAL_TRUE;*/
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/*ctx->current_fps = 300;*/
	ctx->autoflicker_en = KAL_FALSE;
	hs_video_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 720P@240FPS\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	/*ctx->video_mode = KAL_TRUE;*/
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/*ctx->current_fps = 300;*/
	ctx->autoflicker_en = KAL_FALSE;
	slim_video_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/* slim_video */


static kal_uint32 custom1(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom1_setting(ctx);

	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom2.pclk;
	ctx->line_length = imgsensor_info.custom2.linelength;
	ctx->frame_length = imgsensor_info.custom2.framelength;
	ctx->min_frame_length = imgsensor_info.custom2.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom2_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	ctx->pclk = imgsensor_info.custom3.pclk;
	ctx->line_length = imgsensor_info.custom3.linelength;
	ctx->frame_length = imgsensor_info.custom3.framelength;
	ctx->min_frame_length = imgsensor_info.custom3.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	custom3_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/* custom3 */

static kal_uint32 custom4(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. Fullsize center crop 4K@30FPS\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	ctx->pclk = imgsensor_info.custom4.pclk;
	ctx->line_length = imgsensor_info.custom4.linelength;
	ctx->frame_length = imgsensor_info.custom4.framelength;
	ctx->min_frame_length = imgsensor_info.custom4.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	custom4_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/* custom4 */

static kal_uint32 custom5(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 640*480@120FPS\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	ctx->pclk = imgsensor_info.custom5.pclk;
	ctx->line_length = imgsensor_info.custom5.linelength;
	ctx->frame_length = imgsensor_info.custom5.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom5_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/* custom5 */

static kal_uint32 custom6(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	ctx->pclk = imgsensor_info.custom6.pclk;
	ctx->line_length = imgsensor_info.custom6.linelength;
	ctx->frame_length = imgsensor_info.custom6.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	if (!qsc_flag) {
		LOG_INF("write_sensor_QSC Start\n");
		write_sensor_QSC(ctx);
		LOG_INF("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	custom6_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}	/* custom6 */
static int get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i = 0;

	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (i < imgsensor_info.sensor_mode_num &&
			i < ARRAY_SIZE(imgsensor_winsize_info)) {
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
	// LOG_INF("scenario_id = %d\n", scenario_id);

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
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM4] =
		imgsensor_info.custom4_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM5] =
		imgsensor_info.custom5_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 2;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */


	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

	return ERROR_NONE;
}	/*	get_info  */


static int control(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {

	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
		//imgsensor.sensor_mode = scenario_id;
		custom5_15(ctx, image_window, sensor_config_data);
		break;
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
	case SENSOR_SCENARIO_ID_CUSTOM4:
		custom4(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		custom5(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		custom6(ctx, image_window, sensor_config_data);
		break;
	default:
		pr_debug("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control(ctx) */

static kal_uint32 seamless_switch(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	kal_uint32 shutter, kal_uint32 gain,
	kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
	//int k = 0;
	_is_seamless = true;
	memset(_i2c_data, 0x0, sizeof(_i2c_data));
	_size_to_write = 0;
	LOG_INF("%s %d, %d, %d, %d, %d sizeof(_i2c_data) %lu\n",
		__func__, scenario_id, shutter, gain,
		shutter_2ndframe, gain_2ndframe, sizeof(_i2c_data));

	if (scenario_id != SENSOR_SCENARIO_ID_NORMAL_CAPTURE &&
		scenario_id != SENSOR_SCENARIO_ID_CUSTOM4 &&
		scenario_id != SENSOR_SCENARIO_ID_CUSTOM3){
		LOG_INF("Error scenario for %s %d", __func__, scenario_id);
		_is_seamless = false;
		_size_to_write = 0;
		return ERROR_INVALID_SCENARIO_ID;
		}

	_i2c_data[_size_to_write++] = 0x0104;
	_i2c_data[_size_to_write++] = 0x01;

	control(ctx, scenario_id, NULL, NULL);
	if (shutter != 0)
		set_shutter(ctx, shutter);
	if (gain != 0)
		set_gain(ctx, gain);


	_i2c_data[_size_to_write++] = 0x0104;
	_i2c_data[_size_to_write++] = 0;

	LOG_INF("%s _is_seamless %d, _size_to_write %d\n",
		__func__, _is_seamless, _size_to_write);

#if ByPass
		for (k = 0; k < _size_to_write; k += 2)
			LOG_INF("k = %d, 0x%x , 0x%x\n",
				k, _i2c_data[k], _i2c_data[k+1]);
#endif

	imx586_table_write_cmos_sensor(
		ctx,
		_i2c_data,
		_size_to_write);
#if ByPass
		LOG_INF("===========================================\n");
		for (k = 0; k < _size_to_write; k += 2)
			LOG_INF("k = %d, 0x%x , 0x%x\n", k,
				_i2c_data[k], read_cmos_sensor_8(_i2c_data[k]));

#endif

	_is_seamless = false;
	memset(_i2c_data, 0x0, sizeof(_i2c_data));
	_size_to_write = 0;
	LOG_INF("exit\n");
	return ERROR_NONE;
#if ByPass /*havn't change to v4l2 */
	pr_info("seamless switch to scenario %d!\n", scenario_id);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	{
		ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;
		ctx->pclk = imgsensor_info.cap.pclk;
		ctx->line_length = imgsensor_info.cap.linelength;
		ctx->frame_length = imgsensor_info.cap.framelength;
		ctx->min_frame_length = imgsensor_info.cap.framelength;
		ctx->autoflicker_en = KAL_FALSE;

		if (gain != 0) {
			imx586_seamless_capture[3] =
				(gain >> 8) & 0xff;
			imx586_seamless_capture[5] =
				gain & 0xff;
		}
		if (shutter != 0) {
			imx586_seamless_capture[7] =
				(shutter >> 8) & 0xff;
			imx586_seamless_capture[9] =
				shutter & 0xff;
		}

		imx586_table_write_cmos_sensor(ctx, imx586_seamless_capture,
		sizeof(imx586_seamless_capture) / sizeof(kal_uint16));
		}
		break;

	case SENSOR_SCENARIO_ID_CUSTOM4:
	{
	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	ctx->pclk = imgsensor_info.custom4.pclk;
	ctx->line_length = imgsensor_info.custom4.linelength;
	ctx->frame_length = imgsensor_info.custom4.framelength;
	ctx->min_frame_length = imgsensor_info.custom4.framelength;
	ctx->autoflicker_en = KAL_FALSE;

		if (gain != 0) {
			imx586_seamless_custom4[3] =
				(gain >> 8) & 0xff;
			imx586_seamless_custom4[5] =
				gain & 0xff;
	}
	if (shutter != 0) {
		imx586_seamless_custom4[7] =
			(shutter >> 8) & 0xff;
		imx586_seamless_custom4[9] =
			shutter & 0xff;
	}
	 imx586_table_write_cmos_sensor(ctx, imx586_seamless_custom4,
	sizeof(imx586_seamless_custom4) / sizeof(kal_uint16));
	}
	break;
#if ByPass
	case SENSOR_SCENARIO_ID_CUSTOM1:
	{
		pr_info("seamless switch to zoom-in 1.3x!\n");
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
		imgsensor.pclk = imgsensor_info.custom1.pclk;
		imgsensor.line_length = imgsensor_info.custom1.linelength;
		imgsensor.frame_length = imgsensor_info.custom1.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		if (gain != 0) {
			imx586_seamless_custom1[3] =
				(gain >> 8) & 0xff;
			imx586_seamless_custom1[5] =
				gain & 0xff;
		}
		if (shutter != 0) {
			imx586_seamless_custom1[7] =
				(shutter >> 8) & 0xff;
			imx586_seamless_custom1[9] =
				shutter & 0xff;
		}
		imx586_table_write_cmos_sensor(ctx, imx586_seamless_custom1,
		sizeof(imx586_seamless_custom1) / sizeof(kal_uint16));
	}
	break;
#endif
	case SENSOR_SCENARIO_ID_CUSTOM3:
	{
		ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM3;
		ctx->pclk = imgsensor_info.custom3.pclk;
		ctx->line_length = imgsensor_info.custom3.linelength;
		ctx->frame_length = imgsensor_info.custom3.framelength;
		ctx->min_frame_length = imgsensor_info.custom3.framelength;
		ctx->autoflicker_en = KAL_FALSE;
		if (gain != 0) {
			imx586_seamless_custom3[3] =
				(gain >> 8) & 0xff;
			imx586_seamless_custom3[5] =
				gain & 0xff;
		}
		if (shutter != 0) {
			imx586_seamless_custom3[7] =
				(shutter >> 8) & 0xff;
			imx586_seamless_custom3[9] =
				shutter & 0xff;
		}

		 imx586_table_write_cmos_sensor(ctx, imx586_seamless_custom3,
		sizeof(imx586_seamless_custom3) / sizeof(kal_uint16));
	}
	break;
	default:
	{
		pr_info(
		"error! wrong setting in set_seamless_switch = %d",
		scenario_id);
		return 0xff;
	}
	}
#if ByPass
	if (shutter_2ndframe != 0)
		set_shutter(shutter_2ndframe);
	if (gain_2ndframe != 0)
		set_gain(gain_2ndframe);
#endif
	pr_info(
	"done for seamless settings shutter %d  %d gain %d %d !\n",
	shutter, shutter_2ndframe, gain, gain_2ndframe);
#endif
	return 0;
}
static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	pr_debug("framerate = %d\n ", framerate);
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
	set_max_framerate_video(ctx, ctx->current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx,
		kal_bool enable, UINT16 framerate)
{
	if (enable) /*enable auto flicker*/ {
		ctx->autoflicker_en = KAL_TRUE;
		pr_debug("enable: %u fps = %d", (UINT32)enable, framerate);
	} else {
		 /*Cancel Auto flick*/
		ctx->autoflicker_en = KAL_FALSE;
		pr_debug("enable: %u fps = %d", (UINT32)enable, framerate);
	}

	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	DEBUG_LOG(ctx, "scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
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
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
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
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		DEBUG_LOG(ctx,
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
			, framerate, imgsensor_info.cap.max_framerate/10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
			ctx->dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;
			ctx->frame_length =
				imgsensor_info.cap.framelength
				+ ctx->dummy_line;
			ctx->min_frame_length = ctx->frame_length;

		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
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
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
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
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
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
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
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
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
				/ imgsensor_info.custom3.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
		? (frame_length - imgsensor_info.custom3.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom3.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10
				/ imgsensor_info.custom4.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom4.framelength)
		? (frame_length - imgsensor_info.custom4.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom4.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10
				/ imgsensor_info.custom5.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom5.framelength)
		? (frame_length - imgsensor_info.custom5.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom5.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		frame_length = imgsensor_info.custom6.pclk / framerate * 10
				/ imgsensor_info.custom6.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom6.framelength)
		? (frame_length - imgsensor_info.custom6.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom6.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
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
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		pr_debug("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	// pr_debug("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {

	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
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
	case SENSOR_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		*framerate = imgsensor_info.custom6.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_uint32 mode)
{
	if (mode != ctx->test_pattern)
		pr_debug("test_pattern mode: %d\n", mode);
	/*Clear data if not solid color*/
	if (mode != 1) {
		memset(_i2c_data, 0x0, sizeof(_i2c_data));
		_size_to_write = 0;
	}
	if (_size_to_write == 0)
		set_cmos_sensor_8(ctx, 0x0104, 0x01);
	if (mode)
		set_cmos_sensor_8(ctx, 0x0601, mode); /*100% Color bar*/
	else if (ctx->test_pattern)
		set_cmos_sensor_8(ctx, 0x0601, 0x00); /*No pattern*/
	set_cmos_sensor_8(ctx, 0x0104, 0x00);
	commit_write_sensor(ctx);
	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_data(struct subdrv_ctx *ctx, struct mtk_test_pattern_data *data)
{

	DEBUG_LOG(ctx, "test_patterndata R = %x, Gr = %x,Gb = %x,B = %x\n",
		data->Channel_R >> 22, data->Channel_Gr >> 22,
		data->Channel_Gb >> 22, data->Channel_B >> 22);
	set_cmos_sensor_8(ctx, 0x0104, 0x01);
	set_cmos_sensor_8(ctx, 0x0602, (data->Channel_R >> 30) & 0x3);
	set_cmos_sensor_8(ctx, 0x0603, (data->Channel_R >> 22) & 0xff);
	set_cmos_sensor_8(ctx, 0x0604, (data->Channel_Gr >> 30) & 0x3);
	set_cmos_sensor_8(ctx, 0x0605, (data->Channel_Gr >> 22) & 0xff);
	set_cmos_sensor_8(ctx, 0x0606, (data->Channel_B >> 30) & 0x3);
	set_cmos_sensor_8(ctx, 0x0607, (data->Channel_B >> 22) & 0xff);
	set_cmos_sensor_8(ctx, 0x0608, (data->Channel_Gb >> 30) & 0x3);
	set_cmos_sensor_8(ctx, 0x0609, (data->Channel_Gb >> 22) & 0xff);
	//commit_write_sensor(ctx);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(ctx, 0x013a);

	if (temperature <= 0x4F)
		temperature_convert = temperature;
	else if (temperature >= 0x50 && temperature <= 0x7F)
		temperature_convert = 80;
	else if (temperature >= 0x80 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (INT8) temperature;

	/* LOG_INF("temp_c(%d), read_reg(%d)\n", */
	/* temperature_convert, temperature); */

	return temperature_convert;
}

static int feature_control(
		struct subdrv_ctx *ctx,
		MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para,
		UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para
	 *  = (unsigned long long *) feature_para;
	 */
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*pr_debug("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
			SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_R;
			break;
		}
	break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(imx586_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)imx586_ana_gain_table,
			sizeof(imx586_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;

		switch (*feature_data) {
		/* non-binning */
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(feature_data + 2) = BASEGAIN * 16;
			break;
		/* binning */
		default:
			*(feature_data + 2) = imgsensor_info.max_gain;
			break;
		}

		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_GET_MAX_EXP_LINE:
		*(feature_data + 2) =
			imgsensor_info.max_frame_length - imgsensor_info.margin;
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
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
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
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom5.framelength << 16)
				+ imgsensor_info.custom5.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom6.framelength << 16)
				+ imgsensor_info.custom6.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
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
		 /* night_mode((BOOL) *feature_data); */
		break;
	#ifdef VENDOR_EDIT
	case SENSOR_FEATURE_CHECK_MODULE_ID:
		*feature_return_para_32 = imgsensor_info.module_id;
		break;
	#endif
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(ctx, (UINT32) * feature_data);
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
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature(ctx);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(ctx,
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		 get_default_framerate_by_scenario(ctx,
				(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (UINT32)*feature_data);
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN_DATA:
		set_test_pattern_data(ctx, (struct mtk_test_pattern_data *)feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", (UINT32)*feature_data_32);
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", (BOOL)*feature_data_32);
		ctx->ihdr_mode = *feature_data_32;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
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
		case SENSOR_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[8],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[9],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[10],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		// pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			// (UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE: //4000*3000
			imgsensor_pd_info_binning.i4BlockNumX = 496;
			imgsensor_pd_info_binning.i4BlockNumY = 186;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:  //4000*2600
			imgsensor_pd_info_binning.i4BlockNumX = 496;
			imgsensor_pd_info_binning.i4BlockNumY = 162;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2: // 3840*2160
			imgsensor_pd_info_binning.i4BlockNumX = 480;
			imgsensor_pd_info_binning.i4BlockNumY = 134;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3: /* 3840*2160*/
			imgsensor_pd_info_binning.i4BlockNumX = 480;
			imgsensor_pd_info_binning.i4BlockNumY = 134;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4: /* 3840*2160*/
			imgsensor_pd_info_binning.i4BlockNumX = 480;
			imgsensor_pd_info_binning.i4BlockNumY = 134;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6: /* 7680*4320 */
			imgsensor_pd_info_binning.i4BlockNumX = 480;
			imgsensor_pd_info_binning.i4BlockNumY = 134;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO: // 4000*2256
			imgsensor_pd_info_binning.i4BlockNumX = 496;
			imgsensor_pd_info_binning.i4BlockNumY = 140;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		// pr_debug(
		// "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			// (UINT16) *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
#if DPHY_2LANE
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
#endif
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		imx586_get_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		imx586_set_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		ctx->pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(ctx, (UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)),
					(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length(ctx) support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		// pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		// pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			// *feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*feature_return_para_32 = 1; /*BINNING_NONE*/
			break;
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		// pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			// *feature_return_para_32);
		*feature_para_len = 4;

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
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
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
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
break;

	case SENSOR_FEATURE_GET_VC_INFO:
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
#if DPHY_2LANE
			/* no vc */
			break;
#else
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
#endif
		case SENSOR_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[4],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[5],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[6],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			/* no vc */
			break;
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[7],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[8],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		default:
			pr_info("error: get wrong vc_INFO id = %d",
			*feature_data_32);
			break;
		}
	break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to separate 3hdr and remosaic */
		if (ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM3) {
			/*write AWB gain to sensor*/
			feedback_awbgain(ctx, (UINT32)*(feature_data_32 + 1),
					(UINT32)*(feature_data_32 + 2));
		} else {
			imx586_awb_gain(ctx,
				(struct SET_SENSOR_AWB_GAIN *) feature_para);
		}
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
		kal_uint8 index =
			*(((kal_uint8 *)feature_para) + (*feature_para_len));

		imx586_set_lsc_reg_setting(ctx, index, feature_data_16,
					  (*feature_para_len)/sizeof(UINT16));
		}
		break;
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
	{
		if ((feature_data + 1) != NULL) {
			pAeCtrls =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			LOG_INF(
			"warning! no ae_ctrl input");
		}
		if (feature_data == NULL) {
			pr_info("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}

		if (pAeCtrls != NULL) {
			seamless_switch(ctx, (*feature_data),
					*pAeCtrls, *(pAeCtrls + 5),
					*(pAeCtrls + 10), *(pAeCtrls + 15));
		} else {
			seamless_switch(ctx, (*feature_data),
					0, 0, 0, 0);
		}

	}
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		{
		if ((feature_data + 1) != NULL) {
			pScenarios =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			pr_info("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*pScenarios = SENSOR_SCENARIO_ID_CUSTOM3;
			*(pScenarios + 1) = SENSOR_SCENARIO_ID_CUSTOM4;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
			*(pScenarios + 1) = SENSOR_SCENARIO_ID_CUSTOM4;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
			*(pScenarios + 1) = SENSOR_SCENARIO_ID_CUSTOM3;
			break;
#if ByPass
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*pScenarios = SENSOR_SCENARIO_ID_CUSTOM4;
			*(pScenarios + 1) = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
			break;
#endif
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*pScenarios = 0xff;
			break;
		}
		// LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
		// *feature_data, *pScenarios);
	break;
	}
	case SENSOR_FEATURE_SET_FRAMELENGTH:
		set_frame_length(ctx, (UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME:
		set_multi_shutter_frame_length(ctx, (UINT32 *)(*feature_data),
					(UINT16) (*(feature_data + 1)),
					(UINT16) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_PRELOAD_EEPROM_DATA:
		/*get eeprom preloader data*/
		*feature_return_para_32 = ctx->is_read_preload_eeprom;
		*feature_para_len = 4;
		if (ctx->is_read_preload_eeprom != 1)
			read_sensor_Cali(ctx);
		break;
	case SENSOR_FEATURE_GET_CUST_PIXEL_RATE:
			switch (*feature_data) {
			case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
				//416(clk)*2(pixel)*0.95
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1037400000;
				break;
			case SENSOR_SCENARIO_ID_CUSTOM3:
				//416(clk)*4(pixel)*.95
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1580800000;
				break;
			case SENSOR_SCENARIO_ID_CUSTOM4:
				//546(clk)*2(pixel)*0.95
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1037400000;
				break;
			default:
				break;
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
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
		},
	},
	{
			.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x04d8,
			.vsize = 0x05d0,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0a28,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x04d8,
			.vsize = 0x0510,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780,
			.vsize = 0x0438,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x0000,
			.vsize = 0x0000,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0FA0,
			.vsize = 0x08d0,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x04d8,
			.vsize = 0x0460,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust1[] = { //type1 need be checked
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780,
			.vsize = 0x0438,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x0000,
			.vsize = 0x0000,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cust2[] = { //4K_video
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0F00,
			.vsize = 0x0870,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x04b0,
			.vsize = 0x0430,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cust3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1f40,
			.vsize = 0x1770,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x04d8,
			.vsize = 0x05d0,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x0280,
			.vsize = 0x02e0,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0280,
			.vsize = 0x01e0,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1e00,
			.vsize = 0x10e0,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x34,
			.hsize = 0x04b0,
			.vsize = 0x0430,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_vid);
		memcpy(fd->entry, frame_desc_vid, sizeof(frame_desc_vid));
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_hs_vid);
		memcpy(fd->entry, frame_desc_hs_vid, sizeof(frame_desc_hs_vid));
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_slim_vid);
		memcpy(fd->entry, frame_desc_slim_vid, sizeof(frame_desc_slim_vid));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust1);
		memcpy(fd->entry, frame_desc_cust1, sizeof(frame_desc_cust1));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust2);
		memcpy(fd->entry, frame_desc_cust2, sizeof(frame_desc_cust2));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust3);
		memcpy(fd->entry, frame_desc_cust3, sizeof(frame_desc_cust3));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust4);
		memcpy(fd->entry, frame_desc_cust4, sizeof(frame_desc_cust4));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust5);
		memcpy(fd->entry, frame_desc_cust5, sizeof(frame_desc_cust5));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust6);
		memcpy(fd->entry, frame_desc_cust6, sizeof(frame_desc_cust6));
		break;
	default:
		return -1;
	}

	return 0;
}
#endif


static const struct subdrv_ctx defctx = {

	.ana_gain_def = 4 * BASEGAIN,
	.ana_gain_max = 64 * BASEGAIN,
	.ana_gain_min = 1147,//BASEGAIN*1.12(1db)
	.ana_gain_step = 1,
	.exposure_def = 0x3D0,
	/* support long exposure at most 128 times) */
	.exposure_max = (0xffff * 128) - 48,
	.exposure_min = 6,
	.exposure_step = 2,
	.frame_time_delay_frame = 3,
	.margin = 48,
	.max_frame_length = 0xffff,
	.is_streaming = KAL_FALSE,
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 4 * BASEGAIN,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34, /* record current sensor's i2c write id */
	.ae_ctrl_gph_en = 0,
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}
static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	csi_param->legacy_phy = 0;
	csi_param->not_fixed_trail_settle = 0;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM6:
		csi_param->cphy_settle = 69;//0x13;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM2:
		csi_param->cphy_settle = 73;// 0x14;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		csi_param->cphy_settle = 76;//0x15;
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		csi_param->cphy_settle = 84;//0x17;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		csi_param->cphy_settle = 87;//0x18;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		csi_param->cphy_settle = 91;// 0x19;
		break;
	default:
		csi_param->legacy_phy = 1;
		break;
	}
	return 0;
}

static int get_temp(struct subdrv_ctx *ctx, int *temp)
{
	*temp = get_sensor_temperature(ctx) * 1000;
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
	.get_temp = get_temp,
	.get_csi_param = get_csi_param,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_RST, 0, 0},
	{HW_ID_AVDD, 2800000, 0},
	{HW_ID_AVDD1, 1800000, 3},
	{HW_ID_AFVDD, 2800000, 0},
	{HW_ID_DVDD, 1100000, 0},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 1},
	{HW_ID_RST, 1, 3},
};

const struct subdrv_entry imx586_mipi_raw_entry = {
	.name = "imx586_mipi_raw",
	.id = IMX586_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

