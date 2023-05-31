// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 PLATOS5KHM6mipi_Sensor.c
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
#define PFX "PLATOS5KHM6_camera_sensor"


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

#include "platos5khm6_ana_gain_table.h"
#include "platos5khm6mipiraw_Sensor.h"
#include "platos5khm6mipiraw_Sensor_2.h"
#include "platos5khm6_seamless_switch.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#include "async-setting.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define platos5khm6_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u16(__VA_ARGS__)
#define platos5khm6_burst_write_cmos_sensor(...) subdrv_i2c_wr_regs_u16_burst(__VA_ARGS__)
#define LOG_TAG "[platos5khm6]"
#define S5HM6_LOG_INF(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)
#define S5HM6_LOG_DBG(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)

#undef VENDOR_EDIT

#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */

#define ENABLE_PDAF            1
#define SEAMLESS_SWITCH_ENABLE 1
#define EEPROM_SLAVE_ID        0xA2
// common register
#define SENSOR_ID_ADDR                0x0000
#define FRAME_LENGTH_LINES_ADDR       0x0340
#define FRAME_LENGTH_LINES_SHIFT_ADDR 0x0702
#define LINE_LENGTH_PIXEL_ADDR        0x0342
#define COARSE_INTEGRATION_TIME_ADDR  0x0202
#define COARSE_INTEGRATION_TIME_SHIFT_ADDR 0x0704
#define AGAIN_ADDR      0x0204
#define DGAIN_ADDR      0x020E
#define GROUP_HOLD_ADDR 0x0104
#define AWB_R_GAIN_ADDR 0x0D82
#define AWB_G_GAIN_ADDR 0x0D84
#define AWB_B_GAIN_ADDR 0x0D86

//static kal_uint8 pre_is_fullsize = 0;
static kal_uint8 enable_seamless = 0;
static struct SET_SENSOR_AWB_GAIN last_sensor_awb;
static struct setting_workqueue *platos5khm6_workqueue = NULL;
static int g_vendor_id = 0;



static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = PLATOS5KHM6_SENSOR_ID,

	.checksum_value = 0xa4c32546,

	.pre = {
		.pclk = 1640000000,
		.linelength  = 8704,
		.framelength = 6240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1440504000,
	},
	.cap = { /*same preview*/
		.pclk = 1640000000,
		.linelength  = 8704,
		.framelength = 6240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1440504000,
	},
	.normal_video = {
		.pclk = 1640000000,
		.linelength  = 11088,
		.framelength = 4928,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 2252,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1440504000,
	},
	.hs_video = {
		.pclk = 1640000000,
		.linelength  = 5728,
		.framelength = 1192,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 2400,
		.mipi_pixel_rate = 1440504000,
	},
	.slim_video = {
		.pclk = 1640000000,
		.linelength  = 8704,
		.framelength = 6240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1440504000,
	},
	.custom1 = {
		.pclk = 1640000000,
		.linelength  = 8704,
		.framelength = 6240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1440504000,
	},
	.custom2 = {
		.pclk = 1640000000,
		.linelength = 8704,
		.framelength = 1564,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 1200,
		.mipi_pixel_rate = 1440504000,
	},
	.custom3 = {
		.pclk = 1640000000,
		.linelength = 8704,
		.framelength = 3128,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 2252,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 1440504000,
	},
	.custom4 = {
		.pclk = 1640000000,
		.linelength  = 20472,
		.framelength = 9884,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 12000,
		.grabwindow_height = 9000,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 80,
		.mipi_pixel_rate = 1440504000,
	},
	.custom5 = { //in sensor zoom 3X
		.pclk = 1640000000,
		.linelength  = 16896,
		.framelength = 3234,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1440504000,
	},
	.custom6 = {
		.pclk = 1640000000,
		.linelength  = 8704,
		.framelength = 7850,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 240,
		.mipi_pixel_rate = 1440504000,
	},
	.custom7 = {
		.pclk = 1640000000,
		.linelength  = 11088,
		.framelength = 6162,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 2252,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 240,
		.mipi_pixel_rate = 1440504000,
	},
	.margin       = 25, /* sensor framelength & shutter margin */
	.min_shutter  = 12, /* min shutter */
	.min_gain     = BASEGAIN, //1x
	.max_gain     = 64 * BASEGAIN,//40x
	.min_gain_iso = 50,
	.exp_step     = 2,
	.gain_step    = 1,
	.gain_type    = 0,

	.max_frame_length           = 0x56FFFF,	//0x420E40 = 30 * 30 * 6320 + 25 = 5688025
	.ae_shut_delay_frame        = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame     = 2, /* isp gain delay frame for AE cycle */
	.frame_time_delay_frame     = 2, // n+1 effect ,need set 2

	.ihdr_support               = 0, /* 1, support; 0,not support */
	.ihdr_le_firstline          = 0, /* 1,le first ; 0, se first */
	.temperature_support        = 0, /* 1, support; 0,not support */

	.sensor_mode_num        = 12, /* support sensor mode num */
	.cap_delay_frame        = 2, /* enter capture delay frame num */
	.pre_delay_frame        = 2, /* enter preview delay frame num */
	.video_delay_frame      = 2, /* enter video delay frame num */
	.hs_video_delay_frame   = 2,
	.slim_video_delay_frame = 2, /* enter slim video delay frame num */
	.custom1_delay_frame    = 2, /* enter custom1 delay frame num */
	.custom2_delay_frame    = 2, /* enter custom2 delay frame num */
	.custom3_delay_frame    = 2, /* enter custom3 delay frame num */
	.custom4_delay_frame    = 2, /* enter custom4 delay frame num */
	.custom5_delay_frame    = 2, /* enter custom5 delay frame num */
	.custom6_delay_frame    = 2, /* enter custom6 delay frame num */
	.custom7_delay_frame    = 2, /* enter custom7 delay frame num */

	.isp_driving_current      = ISP_DRIVING_6MA,
	.sensor_interface_type    = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type         = MIPI_CPHY, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode   = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gb,
	.mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num            = SENSOR_MIPI_3_LANE,
	.i2c_addr_table           = {0x20, 0xff},
	.i2c_speed                = 1000, /* i2c read/write speed */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[12] = {
	{ 12000, 9000,    0,     0,  12000, 9000,  4000, 3000,    0,    0,  4000, 3000,    0,    0,  4000, 3000}, // Preview
	{ 12000, 9000,    0,     0,  12000, 9000,  4000, 3000,    0,    0,  4000, 3000,    0,    0,  4000, 3000}, // capture
	{ 12000, 9000,    0,  1122,  12000, 6756,  4000, 2252,    0,    0,  4000, 2252,    0,    0,  4000, 2252}, // video
	{ 12000, 9000,  240,  1260,  11520, 6480,  1920, 1080,    0,    0,  1920, 1080,    0,    0,  1920, 1080}, // high speed video 240fps
	{ 12000, 9000,    0,     0,  12000, 9000,  4000, 3000,    0,    0,  4000, 3000,    0,    0,  4000, 3000}, // slim video same as preview
	{ 12000, 9000,    0,     0,  12000, 9000,  4000, 3000,    0,    0,  4000, 3000,    0,    0,  4000, 3000}, // custom1 stereo as Preview
	{ 12000, 9000,  240,  1260,  11520, 6480,  1920, 1080,    0,    0,  1920, 1080,    0,    0,  1920, 1080}, // custom2 as 120fps
	{ 12000, 9000,    0,  1122,  12000, 6756,  4000, 2252,    0,    0,  4000, 2252,    0,    0,  4000, 2252}, // custom3 as 12M @ 60fps
	{ 12000, 9000,    0,     0,  12000, 9000, 12000, 9000,    0,    0, 12000, 9000,    0,    0, 12000, 9000}, // custom4 full size capture
	{ 12000, 9000, 4000,  3000,   4000, 3000,  4000, 3000,    0,    0,  4000, 3000,    0,    0,  4000, 3000}, // custom5 in sensor zoom3x
	{ 12000, 9000,    0,     0,  12000, 9000,  4000, 3000,    0,    0,  4000, 3000,    0,    0,  4000, 3000}, // custom6 as 12M @ 60fps
	{ 12000, 9000,	  0,  1122,  12000, 6756,  4000, 2252,	  0,	0,	4000, 2252,    0,	 0,  4000, 2252}, // custom7 for 16:9 and 24fps
};
#if ENABLE_PDAF
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[4] = {
	// Preview mode setting 2000(pxiel)*1500
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4000, 3000, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 2000, 1500, 0x00, 0x00, 0x0000, 0x0000
	},
	// Video mode setting 2000(pxiel)*1126
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4000, 2252, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 2000, 1126, 0x00, 0x00, 0x0000, 0x0000
	},
	//1080p
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 1920, 1080, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 960, 540, 0x00, 0x00, 0x0000, 0x0000
	},
	// Full crop 12M mode setting 664(pxiel)*500
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4000, 3000, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 1332, 1000, 0x00, 0x00, 0x0000, 0x0000
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info[4] = {
	{
		.i4OffsetX	= 0,
		.i4OffsetY	= 0,
		.i4PitchX	= 2,
		.i4PitchY	= 4,
		.i4PairNum	= 1,
		.i4SubBlkW	= 2,
		.i4SubBlkH	= 4,
		.i4BlockNumX = 2000,
		.i4BlockNumY = 750,
		.iMirrorFlip = IMAGE_HV_MIRROR,
		.i4PosL = {
			{1, 0}, //{3, 0},
		},
		.i4PosR = {
			{0, 0}, //{2, 0},
		},
	},
	{
		.i4OffsetX	= 0,
		.i4OffsetY	= 0,
		.i4PitchX	= 2,
		.i4PitchY	= 4,
		.i4PairNum	= 1,
		.i4SubBlkW	= 2,
		.i4SubBlkH	= 4,
		.i4BlockNumX = 2000,
		.i4BlockNumY = 563,
		.iMirrorFlip = IMAGE_HV_MIRROR,
		.i4PosL = {
			{1, 0}, //{3, 0},
		},
		.i4PosR = {
			{0, 0}, //{2, 0},
		},
		.i4Crop = {
			{0,0},{0,0},{0,374},{0,0},{0,0},{0,0},{0,0},{0,374},{0,0},
			{0,0},{0,0},{0,374},
		},
	},
	{//1080P
		.i4OffsetX	= 0,
		.i4OffsetY	= 0,
		.i4PitchX	= 2,
		.i4PitchY	= 4,
		.i4PairNum	= 1,
		.i4SubBlkW	= 2,
		.i4SubBlkH	= 4,
		.i4BlockNumX = 960,
		.i4BlockNumY = 270,
		.iMirrorFlip = IMAGE_HV_MIRROR,
		.i4PosL = {
			{1, 0}, //{3, 0},
		},
		.i4PosR = {
			{0, 0}, //{2, 0},
		},
	},
	{//full crop
		.i4OffsetX	= 1,
		.i4OffsetY	= 0,
		.i4PitchX	= 12,
		.i4PitchY	= 12,
		.i4PairNum	= 8,
		.i4SubBlkW	= 3,
		.i4SubBlkH	= 6,
		.i4BlockNumX = 333,
		.i4BlockNumY = 250,
		.iMirrorFlip = IMAGE_HV_MIRROR,
		.i4PosL = {
			{3, 1}, {6, 1}, {9, 1}, {12, 1}, {3, 7}, {6, 7}, {9, 7}, {12, 7},
		},
		.i4PosR = {
			{2, 1}, {5, 1}, {8, 1}, {11, 1}, {2, 7}, {5, 7}, {8, 7}, {11, 7},
		},
		.i4Crop = {
			{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
			{4000,3000},{0,0},{0,0},
		},
	},
};

static void platos5khm6_get_pdaf_reg_setting(struct subdrv_ctx *ctx, MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor(ctx, regDa[idx]);
		S5HM6_LOG_INF("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void platos5khm6_set_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor(ctx, regDa[idx], regDa[idx + 1]);
		S5HM6_LOG_INF("%x %x", regDa[idx], regDa[idx+1]);
	}
}
#endif

static kal_uint16 platos5khm6_async_set_dummy_setting[4] = {0};
static kal_int32 platos5khm6_async_set_dummy(struct subdrv_ctx *ctx,
                                 kal_uint16 *para, kal_uint32 len)
{
	kal_uint16 line_length  =  platos5khm6_async_set_dummy_setting[0];
	kal_uint16 frame_length =  platos5khm6_async_set_dummy_setting[1];
	kal_uint16 dummy_line   =  platos5khm6_async_set_dummy_setting[2];
	kal_uint16 dummy_pixel  =  platos5khm6_async_set_dummy_setting[3];

	S5HM6_LOG_DBG("dummyline = %d, dummypixels = %d \n", dummy_line, dummy_pixel);
	write_cmos_sensor(ctx, LINE_LENGTH_PIXEL_ADDR, line_length & 0xFFFF);
	write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, frame_length & 0xFFFF);
	return 0;
}

static void set_dummy(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_DBG("dummyline = %d, dummypixels = %d \n", ctx->dummy_line, ctx->dummy_pixel);
	if (ctx->is_streaming == false) {
		platos5khm6_async_set_dummy_setting[0] = ctx->line_length & 0xFFFF;
		platos5khm6_async_set_dummy_setting[1] = ctx->frame_length & 0xFFFF;
		platos5khm6_async_set_dummy_setting[2] = ctx->dummy_line & 0xFFFF;
		platos5khm6_async_set_dummy_setting[3] = ctx->dummy_pixel & 0xFFFF;
		// ASync set_dummy
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_async_set_dummy_setting",
					EXP_SETTING,
					platos5khm6_async_set_dummy,
					ctx,
					platos5khm6_async_set_dummy_setting,
					sizeof(platos5khm6_async_set_dummy_setting) / sizeof(kal_uint16));
	} else {
		write_cmos_sensor(ctx, LINE_LENGTH_PIXEL_ADDR, ctx->line_length & 0xFFFF);
		write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, ctx->frame_length & 0xFFFF);
	}
}	/*	set_dummy  */



static void set_max_framerate(struct subdrv_ctx *ctx,
		UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = ctx->frame_length;

	S5HM6_LOG_INF(
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
	set_dummy(ctx);
}	/*	set_max_framerate  */

static kal_uint16 platos5khm6_async_write_shutter_setting[5] = {0};
static kal_int32 platos5khm6_async_write_shutter(struct subdrv_ctx *ctx,
                                 kal_uint16 *para, kal_uint32 len)
{
	kal_uint32 frame_length = (platos5khm6_async_write_shutter_setting[0] << 16) |
								platos5khm6_async_write_shutter_setting[1];
	kal_uint32 shutter      = (platos5khm6_async_write_shutter_setting[2] << 16) |
								platos5khm6_async_write_shutter_setting[3];
	kal_uint16 Rshift       =  platos5khm6_async_write_shutter_setting[4];

	// Update Shutter
	write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
	write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, (frame_length >> Rshift) & 0xFFFF);
	write_cmos_sensor(ctx, COARSE_INTEGRATION_TIME_ADDR, (shutter >> Rshift) & 0xFFFF);
	write_cmos_sensor(ctx, FRAME_LENGTH_LINES_SHIFT_ADDR, (Rshift << 8) & 0xFFFF);
	write_cmos_sensor(ctx, COARSE_INTEGRATION_TIME_SHIFT_ADDR, (Rshift << 8) & 0xFFFF);
	write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);
	S5HM6_LOG_DBG("Rshift =%d, shutter =%d, framelength =%d\n", Rshift, shutter, frame_length);

	return 0;
}

static kal_uint16 platos5khm6_async_write_gain_setting[3] = {0};
static kal_int32 platos5khm6_async_write_gain(struct subdrv_ctx *ctx,
                                 kal_uint16 *para, kal_uint32 len)
{
	kal_uint32 reg_gain =  platos5khm6_async_write_gain_setting[0];
	kal_uint32 gain     = (platos5khm6_async_write_gain_setting[1] << 16) |
								platos5khm6_async_write_gain_setting[2];

	// Update Shutter
	write_cmos_sensor(ctx, AGAIN_ADDR, reg_gain);
	S5HM6_LOG_DBG("gain = %d, reg_gain = 0x%x\n ", gain, reg_gain);

	return 0;
}

static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 Rshift;

	if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	// Framelength should be an even number
	shutter = (shutter >> 1) << 1;
	ctx->frame_length = (ctx->frame_length >> 1) << 1;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10
				/ ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
			write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, ctx->frame_length & 0xFFFF);
			write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);
		}
	}

	if (shutter & 0xC00000) {
		Rshift = 7;
	} else if (shutter & 0xFFFF0000) {
		Rshift = 6;
	} else {
		Rshift = 0;
	}

	if (ctx->is_streaming == false) {
		platos5khm6_async_write_shutter_setting[0] = (ctx->frame_length >> 16) & 0xFFFF;
		platos5khm6_async_write_shutter_setting[1] = ctx->frame_length & 0xFFFF;

		platos5khm6_async_write_shutter_setting[2] = (shutter >> 16) & 0xFFFF;
		platos5khm6_async_write_shutter_setting[3] = shutter & 0xFFFF;

		platos5khm6_async_write_shutter_setting[4] = Rshift;

		// ASync write shutter
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_async_write_shutter_setting",
					EXP_SETTING,
					platos5khm6_async_write_shutter,
					ctx,
					platos5khm6_async_write_shutter_setting,
					sizeof(platos5khm6_async_write_shutter_setting) / sizeof(kal_uint16));
	} else {
		// Update Shutter
		write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
		write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, (ctx->frame_length >> Rshift) & 0xFFFF);
		write_cmos_sensor(ctx, COARSE_INTEGRATION_TIME_ADDR, (shutter >> Rshift) & 0xFFFF);
		write_cmos_sensor(ctx, FRAME_LENGTH_LINES_SHIFT_ADDR, (Rshift << 8) & 0xFFFF);
		write_cmos_sensor(ctx, COARSE_INTEGRATION_TIME_SHIFT_ADDR, (Rshift << 8) & 0xFFFF);
		write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);
		S5HM6_LOG_DBG("shutter =%d, framelength =%d\n", shutter, ctx->frame_length);
	}

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
	write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
	write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, ctx->frame_length & 0xFFFF);
	write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);

	S5HM6_LOG_INF("Framelength: set=%d/input=%d/min=%d, auto_extend=%d\n",
		ctx->frame_length, frame_length, ctx->min_frame_length,
		read_cmos_sensor_8(ctx, 0x0350));
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
	kal_uint16 Rshift;

	ctx->shutter = shutter;

	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;

	ctx->frame_length = ctx->frame_length + dummy_line;

	if (shutter > ctx->frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
			? (imgsensor_info.max_frame_length - imgsensor_info.margin)
			: shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10 /
				ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
			write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, ctx->frame_length & 0xFFFF);
			write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);
		}
	}

	if (shutter & 0xFFFF0000) {
		Rshift = 6;
	}
	else {
		Rshift = 0;
	}

	// Update Shutter
	write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
	write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, (ctx->frame_length >> Rshift) & 0xFFFF);
	write_cmos_sensor(ctx, COARSE_INTEGRATION_TIME_ADDR, (shutter >> Rshift) & 0xFFFF);
	write_cmos_sensor(ctx, FRAME_LENGTH_LINES_SHIFT_ADDR, (Rshift << 8) & 0xFFFF);
	write_cmos_sensor(ctx, COARSE_INTEGRATION_TIME_SHIFT_ADDR, (Rshift << 8) & 0xFFFF);
	write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);
	S5HM6_LOG_DBG("set shutter =%d, framelength =%d\n", shutter, ctx->frame_length);

}	/* set_shutter_frame_length */

static void set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint16 *shutters, kal_uint16 shutter_cnt,
				kal_uint16 frame_length)
{
	if (shutter_cnt == 1) {
		ctx->shutter = shutters[0];

		if (shutters[0] > ctx->min_frame_length - imgsensor_info.margin)
			ctx->frame_length = shutters[0] + imgsensor_info.margin;
		else
			ctx->frame_length = ctx->min_frame_length;
		if (frame_length > ctx->frame_length)
			ctx->frame_length = frame_length;
		if (ctx->frame_length > imgsensor_info.max_frame_length)
			ctx->frame_length = imgsensor_info.max_frame_length;
		if (shutters[0] < imgsensor_info.min_shutter)
			shutters[0] = imgsensor_info.min_shutter;

		write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
		write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, ctx->frame_length);
		write_cmos_sensor(ctx, COARSE_INTEGRATION_TIME_ADDR, shutters[0]);
		write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);

		S5HM6_LOG_DBG("shutter =%d, framelength =%d\n",
			shutters[0], ctx->frame_length);
	}
}


static kal_uint32 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	return (kal_uint32) gain / 32;
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
static kal_uint16 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{
	kal_uint32 reg_gain, max_gain, min_gain;

	max_gain = imgsensor_info.max_gain;//setuphere for mode use
	min_gain = imgsensor_info.min_gain;//setuphere for mode use

	if (ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM4 ||//16x for full mode
			ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM5) {
		max_gain = 16 * BASEGAIN;
	}

	if (gain < min_gain || gain > max_gain) {
		S5HM6_LOG_INF("Error max gain setting: %d Should between %d & %d\n",
			gain, min_gain, max_gain);
		if (gain < min_gain)
			gain = min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;

	if (ctx->is_streaming == false) {
		platos5khm6_async_write_gain_setting[0] = (kal_uint16)reg_gain;
		platos5khm6_async_write_gain_setting[1] = (gain << 16) & 0xFFFF;
		platos5khm6_async_write_gain_setting[2] =  gain & 0xFFFF;

		// ASync write gain
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_async_write_gain_setting",
					EXP_SETTING,
					platos5khm6_async_write_gain,
					ctx,
					platos5khm6_async_write_gain_setting,
					sizeof(platos5khm6_async_write_gain_setting) / sizeof(kal_uint16));
	} else {
		write_cmos_sensor(ctx, AGAIN_ADDR, reg_gain);
		S5HM6_LOG_DBG("gain = %d, reg_gain = 0x%x, max_gain:0x%x\n ",
			gain, reg_gain, max_gain);
	}

	return gain;
} /* set_gain */

static kal_uint16 platos5khm6_async_awb_gain_setting[6] = {0};
static kal_int32 platos5khm6_async_awb_gain(struct subdrv_ctx *ctx,
                                 kal_uint16 *para, kal_uint32 len)
{
	kal_uint32 r_Gain = (platos5khm6_async_awb_gain_setting[0] << 16) |
								platos5khm6_async_awb_gain_setting[1];
	kal_uint32 g_Gain = (platos5khm6_async_awb_gain_setting[2] << 16) |
								platos5khm6_async_awb_gain_setting[3];
	kal_uint32 b_Gain = (platos5khm6_async_awb_gain_setting[4]<< 16) |
								platos5khm6_async_awb_gain_setting[5];

	// Update Shutter
	write_cmos_sensor(ctx, AWB_R_GAIN_ADDR, r_Gain * 2);
	write_cmos_sensor(ctx, AWB_G_GAIN_ADDR, g_Gain * 2);
	write_cmos_sensor(ctx, AWB_B_GAIN_ADDR, b_Gain * 2);
	S5HM6_LOG_DBG("write awb gain r:g:b %d:%d:%d \n", r_Gain, g_Gain, b_Gain);
	return 0;
}

static kal_uint32 platos5khm6_awb_gain(struct subdrv_ctx *ctx,
		struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	MUINT32 r_Gain = pSetSensorAWB->ABS_GAIN_R;
	MUINT32 g_Gain = pSetSensorAWB->ABS_GAIN_GR;
	MUINT32 b_Gain = pSetSensorAWB->ABS_GAIN_B;

	if (pSetSensorAWB->ABS_GAIN_R  == 0 ||
	    pSetSensorAWB->ABS_GAIN_B  == 0 ||
	    pSetSensorAWB->ABS_GAIN_GR == 0 ||
	    pSetSensorAWB->ABS_GAIN_GB == 0)
		return ERROR_NONE;
	if(!ctx->is_streaming){
		platos5khm6_async_awb_gain_setting[0] = (r_Gain << 16) & 0xFFFF;
		platos5khm6_async_awb_gain_setting[1] = r_Gain  & 0xFFFF;
		platos5khm6_async_awb_gain_setting[2] = (g_Gain << 16) & 0xFFFF;
		platos5khm6_async_awb_gain_setting[3] = g_Gain	& 0xFFFF;
		platos5khm6_async_awb_gain_setting[4] = (b_Gain << 16) & 0xFFFF;
		platos5khm6_async_awb_gain_setting[5] = b_Gain  & 0xFFFF;

		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_async_awb_gain_setting",
					EXP_SETTING,
					platos5khm6_async_awb_gain,
					ctx,
					platos5khm6_async_awb_gain_setting,
					sizeof(platos5khm6_async_awb_gain_setting) / sizeof(kal_uint16));

	}else{
		write_cmos_sensor(ctx, AWB_R_GAIN_ADDR, r_Gain * 2);
		write_cmos_sensor(ctx, AWB_G_GAIN_ADDR, g_Gain * 2);
		write_cmos_sensor(ctx, AWB_B_GAIN_ADDR, b_Gain * 2);

		S5HM6_LOG_DBG("write awb gain r:g:b %d:%d:%d \n", r_Gain, g_Gain, b_Gain);
	}
	return ERROR_NONE;
}


static void platos5khm6_set_lsc_reg_setting(struct subdrv_ctx *ctx,
		kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{

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
	int num = 0;
	S5HM6_LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	wait_workqueue_done(platos5khm6_workqueue);
	S5HM6_LOG_INF("setting done !\n");

	write_cmos_sensor(ctx, 0xFCFC, 0x4000);
	if (enable) {
		write_cmos_sensor(ctx, 0x0100, 0x0103);
		ctx->is_streaming = true;
	} else {
		write_cmos_sensor(ctx, 0x0100, 0x0000);
		while(read_cmos_sensor_8(ctx,0x0005)!=0xFF && num<500){
			num++;
			mdelay(1);
			S5HM6_LOG_INF("0x0005 != %d",read_cmos_sensor_8(ctx,0x0005));
		}
		ctx->is_streaming = false;
	}
	return ERROR_NONE;
}

static kal_int32 platos5khm6_fast_write_cmos_sensor(struct subdrv_ctx *ctx,
                                 kal_uint16 *para, kal_uint32 len)
{
	kal_uint32 burst_thr     = 100;
	kal_uint32 IDX           = 0;
	kal_uint32 table_pos_sta = 0;
	kal_uint32 table_len     = 0;
	kal_uint32 burst_pos_sta = 0;
	kal_uint32 burst_len     = 0;
	kal_uint32 need_write    = 0;
	kal_uint32 last_num      = 0;
	S5HM6_LOG_INF("+\n");

	while (len >= IDX+2) {
		if (burst_len == 0) {
			burst_pos_sta = IDX;
		}
		if (table_len == 0) {
			table_pos_sta = IDX;
		}
		if(len > IDX+2){
			burst_len += 2;
			if (para[IDX]!=(para[IDX+2]-2)) {
				if(burst_len > burst_thr){
					need_write = 1;
				}else{
					if(table_len == 0){
						table_pos_sta = burst_pos_sta;
					}
					table_len += burst_len;
					burst_len = 0;
				}
			}
			if (len == (IDX + 4)) {
				/* the end */
				need_write = 1;
				if(para[IDX]==(para[IDX+2]-2)){
					burst_len += 2;
					last_num = 1;
					if(burst_len <= burst_thr){
						if(table_len == 0){
							table_pos_sta = burst_pos_sta;
						}
						table_len += burst_len;
						burst_len = 0;
					}
				}
			}
		}
		else if(len == (IDX + 2)&&last_num == 0){
			if (table_len == 0) {
				table_pos_sta = IDX;
			}
			table_len += 2;
			need_write = 1;
		}

		if (need_write) {
			if (table_len > 0) {
				/* Find the sequence that requires Table Mode */
				platos5khm6_table_write_cmos_sensor(ctx, para + table_pos_sta, table_len);
				S5HM6_LOG_INF("table_pos_sta = %d, table_len = %d\n", table_pos_sta, table_len);
				table_len = 0;
			}
			if (burst_len > 0){
				/* Find the sequence that requires Burst Mode */
				platos5khm6_burst_write_cmos_sensor(ctx, para + burst_pos_sta, burst_len);
				S5HM6_LOG_INF("burst_pos_sta = %d, burst_len = %d\n", burst_pos_sta, burst_len);
				burst_len = 0;
			}
			need_write = 0;
		}
		IDX += 2;
	}
	S5HM6_LOG_INF("len = %d -\n", len);
	return 0;
}

static int read_vendor_id(struct i2c_client *client, u16 a_u2Addr)
{
	u8 vendorID = 0xFF;
	u8 last_addr = client->addr;
	int i4RetValue = 0;
	char puReadCmd[2] = {(char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF)};

	client->addr = EEPROM_SLAVE_ID;
	client->addr = client->addr >> 1;

	i4RetValue = i2c_master_send(client, puReadCmd, 2);
	if (i4RetValue < 0) {
		S5HM6_LOG_INF(" I2C write failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(client, (char *)&vendorID, 1);
	if (i4RetValue != 1) {
		S5HM6_LOG_INF(" I2C read failed!!\n");
		return -1;
	}
	S5HM6_LOG_INF("vendorID = %d",vendorID);
	client->addr = last_addr;

	return vendorID;
}

static kal_int32 platos5khm6_async_sensor_init(struct subdrv_ctx *ctx,
                                 kal_uint16 *para, kal_uint32 len)
{

	write_cmos_sensor(ctx, 0xFCFC, 0x4000);
	write_cmos_sensor(ctx, 0x0000, 0x0190);
	write_cmos_sensor(ctx, 0x0000, 0x1AD6);
	write_cmos_sensor(ctx, 0xFCFC, 0x4000);
	write_cmos_sensor(ctx, 0x6010, 0x0001);
	mdelay(30);
	write_cmos_sensor(ctx, 0x6218, 0xE9C0);
	write_cmos_sensor(ctx, 0xF468, 0x0000);
	write_cmos_sensor(ctx, 0x0136, 0x1800);
	//Tnp
	platos5khm6_fast_write_cmos_sensor(ctx, para, len);
	mdelay(1);

	return 0;
}

static kal_int32 __platos5khm6_table_write_cmos_sensor(struct subdrv_ctx *ctx,
                                 kal_uint16 *para, kal_uint32 len)
{
	platos5khm6_table_write_cmos_sensor(ctx, para, len);

	return 0;
}

static void sensor_init(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	g_vendor_id = read_vendor_id(ctx->i2c_client,0x01);

	// Tnp
	if(g_vendor_id == 7){//ofilm
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_tnp_setting",
					INIT_SETTING,
					platos5khm6_async_sensor_init,
					ctx,
					platos5khm6_tnp_setting,
					sizeof(platos5khm6_tnp_setting) / sizeof(kal_uint16));

		// Global
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_init_setting",
					INIT_SETTING,
					platos5khm6_fast_write_cmos_sensor,
					ctx,
					platos5khm6_init_setting,
					sizeof(platos5khm6_init_setting) / sizeof(kal_uint16));
	}
	else{//semco
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_tnp_setting",
					INIT_SETTING,
					platos5khm6_async_sensor_init,
					ctx,
					platos5khm6_tnp_setting,
					sizeof(platos5khm6_tnp_setting) / sizeof(kal_uint16));

		// Global
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_init_setting",
					INIT_SETTING,
					platos5khm6_fast_write_cmos_sensor,
					ctx,
					platos5khm6_init_setting,
					sizeof(platos5khm6_init_setting) / sizeof(kal_uint16));
	}
	
	S5HM6_LOG_INF("-\n");
}	/* sensor_init */

static void preview_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	if(g_vendor_id == 7){//ofilm
		if(enable_seamless){
			create_and_queue_setting_work(
						platos5khm6_workqueue,
						"platos5khm6_fmc_setting",
						RES_SETTING,
						platos5khm6_fast_write_cmos_sensor,
						ctx,
						platos5khm6_fmc_setting,
						sizeof(platos5khm6_fmc_setting) / sizeof(kal_uint16));
		}
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_preview_setting",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_preview_setting,
					sizeof(platos5khm6_preview_setting) / sizeof(kal_uint16));
	}
	else{
		if(enable_seamless){
			create_and_queue_setting_work(
						platos5khm6_workqueue,
						"platos5khm6_fmc_setting_2",
						RES_SETTING,
						platos5khm6_fast_write_cmos_sensor,
						ctx,
						platos5khm6_fmc_setting_2,
						sizeof(platos5khm6_fmc_setting_2) / sizeof(kal_uint16));
		}	
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_preview_setting_2",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_preview_setting_2,
					sizeof(platos5khm6_preview_setting_2) / sizeof(kal_uint16));

	}
	S5HM6_LOG_INF("-\n");
} /* preview_setting */

static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	S5HM6_LOG_INF("+\n");
	if(g_vendor_id == 7){//ofilm
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_fmc_108M_setting",
					RES_SETTING,
					platos5khm6_fast_write_cmos_sensor,
					ctx,
					platos5khm6_fmc_108M_setting,
					sizeof(platos5khm6_fmc_108M_setting) / sizeof(kal_uint16));

		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_preview_setting",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_preview_setting,
					sizeof(platos5khm6_preview_setting) / sizeof(kal_uint16));
	}
	else{
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_fmc_108M_setting",
					RES_SETTING,
					platos5khm6_fast_write_cmos_sensor,
					ctx,
					platos5khm6_fmc_108M_setting,
					sizeof(platos5khm6_fmc_108M_setting) / sizeof(kal_uint16));
	
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_preview_setting_2",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_preview_setting_2,
					sizeof(platos5khm6_preview_setting_2) / sizeof(kal_uint16));
	}
	S5HM6_LOG_INF("-\n");
}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	S5HM6_LOG_INF("+\n");
	if(g_vendor_id == 7){//ofilm
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_normal_video_setting",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
						ctx,
					platos5khm6_normal_video_setting,
					sizeof(platos5khm6_normal_video_setting) / sizeof(kal_uint16));
	}
	else{
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_normal_video_setting_2",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_normal_video_setting_2,
					sizeof(platos5khm6_normal_video_setting_2) / sizeof(kal_uint16));
	}
	S5HM6_LOG_INF("-\n");
}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	if(g_vendor_id == 7){//ofilm
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_hs_video_setting",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_hs_video_setting,
					sizeof(platos5khm6_hs_video_setting) / sizeof(kal_uint16));
	}
	else{
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_hs_video_setting_2",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_hs_video_setting_2,
					sizeof(platos5khm6_hs_video_setting_2) / sizeof(kal_uint16));
	}
	S5HM6_LOG_INF("-\n");
}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	preview_setting(ctx);
	S5HM6_LOG_INF("-\n");
}

static void custom1_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	preview_setting(ctx);
	S5HM6_LOG_INF("-\n");
}

static void custom2_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	if(g_vendor_id == 7){//ofilm
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_custom2_setting",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_custom2_setting,
					sizeof(platos5khm6_custom2_setting) / sizeof(kal_uint16));
	}
	else{
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_custom2_setting_2",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_custom2_setting_2,
					sizeof(platos5khm6_custom2_setting_2) / sizeof(kal_uint16));
	}
	S5HM6_LOG_INF("+\n");
}

static void custom3_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	if(g_vendor_id == 7){//ofilm
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_custom3_setting",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_custom3_setting,
					sizeof(platos5khm6_custom3_setting) / sizeof(kal_uint16));
	}
	else{
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_custom3_setting_2",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_custom3_setting_2,
					sizeof(platos5khm6_custom3_setting_2) / sizeof(kal_uint16));
	}
	S5HM6_LOG_INF("-\n");
}

static void custom4_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	if(g_vendor_id == 7){//ofilm
	create_and_queue_setting_work(
				platos5khm6_workqueue,
				"platos5khm6_init_setting",
				RES_SETTING,
				__platos5khm6_table_write_cmos_sensor,
				ctx,
				platos5khm6_init_setting,
				sizeof(platos5khm6_init_setting) / sizeof(kal_uint16));
	create_and_queue_setting_work(
				platos5khm6_workqueue,
				"platos5khm6_custom4_setting",
				RES_SETTING,
				__platos5khm6_table_write_cmos_sensor,
				ctx,
				platos5khm6_custom4_setting,
				sizeof(platos5khm6_custom4_setting) / sizeof(kal_uint16));
	}
	else{	
		create_and_queue_setting_work(
					platos5khm6_workqueue,
					"platos5khm6_custom4_setting_2",
					RES_SETTING,
					__platos5khm6_table_write_cmos_sensor,
					ctx,
					platos5khm6_custom4_setting_2,
					sizeof(platos5khm6_custom4_setting_2) / sizeof(kal_uint16));
	}
	S5HM6_LOG_INF("-\n");
}

static void custom5_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	S5HM6_LOG_INF("enable seamless switch setting\n");
	// Fast Mode Change
	if(g_vendor_id == 7){//ofilm
	create_and_queue_setting_work(
				platos5khm6_workqueue,
				"platos5khm6_fmc_setting",
				RES_SETTING,
				platos5khm6_fast_write_cmos_sensor,
				ctx,
				platos5khm6_fmc_setting,
				sizeof(platos5khm6_fmc_setting) / sizeof(kal_uint16));

	create_and_queue_setting_work(
				platos5khm6_workqueue,
				"platos5khm6_custom5_setting",
				RES_SETTING,
				__platos5khm6_table_write_cmos_sensor,
				ctx,
				platos5khm6_custom5_setting,
				sizeof(platos5khm6_custom5_setting) / sizeof(kal_uint16));
	}
	else{
	create_and_queue_setting_work(
				platos5khm6_workqueue,
				"platos5khm6_fmc_setting_2",
				RES_SETTING,
				platos5khm6_fast_write_cmos_sensor,
				ctx,
				platos5khm6_fmc_setting_2,
				sizeof(platos5khm6_fmc_setting_2) / sizeof(kal_uint16));

	create_and_queue_setting_work(
				platos5khm6_workqueue,
				"platos5khm6_custom5_setting_2",
				RES_SETTING,
				__platos5khm6_table_write_cmos_sensor,
				ctx,
				platos5khm6_custom5_setting_2,
				sizeof(platos5khm6_custom5_setting_2) / sizeof(kal_uint16));
	}
	S5HM6_LOG_INF("-\n");
}

static void custom6_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	preview_setting(ctx);
	S5HM6_LOG_INF("-\n");
}

static void custom7_setting(struct subdrv_ctx *ctx)
{
	S5HM6_LOG_INF("+\n");
	normal_video_setting(ctx,ctx->current_fps);
	S5HM6_LOG_INF("-\n");
}

static kal_uint32 return_sensor_id(struct subdrv_ctx *ctx)
{
	kal_uint32 sensor_id = 0;

	sensor_id = ((read_cmos_sensor_8(ctx, SENSOR_ID_ADDR) << 8) | read_cmos_sensor_8(ctx, SENSOR_ID_ADDR + 1));

	S5HM6_LOG_INF("[%s] sensor_id: 0x%x", __func__, sensor_id);

	return sensor_id;
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
			*sensor_id = return_sensor_id(ctx);

			if (*sensor_id == imgsensor_info.sensor_id) {
				S5HM6_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			S5HM6_LOG_INF("Read sensor id fail, id: 0x%x\n",
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
	kal_uint32 read_register = 0x0000;

	S5HM6_LOG_INF("+\n");
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	read_register=read_cmos_sensor(ctx, 0x0002);
	S5HM6_LOG_DBG("read_register(0x0002) = 0x%x ",read_register);
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = return_sensor_id(ctx);

			if (sensor_id == imgsensor_info.sensor_id) {
				S5HM6_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			S5HM6_LOG_INF("Read sensor id fail, id: 0x%x\n",
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

	last_sensor_awb.ABS_GAIN_GR = 0;
	last_sensor_awb.ABS_GAIN_R  = 0;
	last_sensor_awb.ABS_GAIN_B  = 0;
	last_sensor_awb.ABS_GAIN_GB = 0;

	platos5khm6_workqueue = create_setting_workqueue("platos5khm6_workqueue");

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

	S5HM6_LOG_DBG("-\n");

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
	S5HM6_LOG_INF("E\n");
	/* No Need to implement this function */
	streaming_control(ctx, KAL_FALSE);
	destroy_setting_workqueue(platos5khm6_workqueue);

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
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);
	return ERROR_NONE;
} /* preview */
static kal_uint32 custom8_15(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

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
	S5HM6_LOG_INF("E\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		S5HM6_LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			ctx->current_fps,
			imgsensor_info.cap.max_framerate / 10);
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	capture_setting(ctx, ctx->current_fps);

	return ERROR_NONE;
}	/* capture(ctx) */
static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	normal_video_setting(ctx, ctx->current_fps);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

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

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

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

	return ERROR_NONE;
}	/* slim_video */


static kal_uint32 custom1(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom1_setting(ctx);


	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom2.pclk;
	ctx->line_length = imgsensor_info.custom2.linelength;
	ctx->frame_length = imgsensor_info.custom2.framelength;
	ctx->min_frame_length = imgsensor_info.custom2.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom2_setting(ctx);

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	ctx->pclk = imgsensor_info.custom3.pclk;
	ctx->line_length = imgsensor_info.custom3.linelength;
	ctx->frame_length = imgsensor_info.custom3.framelength;
	ctx->min_frame_length = imgsensor_info.custom3.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom3_setting(ctx);

	return ERROR_NONE;
}	/* custom3 */

static kal_uint32 custom4(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	ctx->pclk = imgsensor_info.custom4.pclk;
	ctx->line_length = imgsensor_info.custom4.linelength;
	ctx->frame_length = imgsensor_info.custom4.framelength;
	ctx->min_frame_length = imgsensor_info.custom4.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom4_setting(ctx);
	if (last_sensor_awb.ABS_GAIN_GR != 0 &&
	    last_sensor_awb.ABS_GAIN_R  != 0 &&
	    last_sensor_awb.ABS_GAIN_B  != 0) {
		S5HM6_LOG_INF("custom4 write awb gain r:g:b %d:%d:%d \n",
						last_sensor_awb.ABS_GAIN_R,
						last_sensor_awb.ABS_GAIN_GR,
						last_sensor_awb.ABS_GAIN_B);
		platos5khm6_awb_gain(ctx, &last_sensor_awb);
	}

	return ERROR_NONE;
}	/* custom4 */

static kal_uint32 custom5(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	ctx->pclk = imgsensor_info.custom5.pclk;
	ctx->line_length = imgsensor_info.custom5.linelength;
	ctx->frame_length = imgsensor_info.custom5.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom5_setting(ctx);

	return ERROR_NONE;
}	/* custom5 */

static kal_uint32 custom6(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	ctx->pclk = imgsensor_info.custom6.pclk;
	ctx->line_length = imgsensor_info.custom6.linelength;
	ctx->frame_length = imgsensor_info.custom6.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom6_setting(ctx);

	return ERROR_NONE;
}	/* custom6 */

static kal_uint32 custom7(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5HM6_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM7;
	ctx->pclk = imgsensor_info.custom7.pclk;
	ctx->line_length = imgsensor_info.custom7.linelength;
	ctx->frame_length = imgsensor_info.custom7.framelength;
	ctx->min_frame_length = imgsensor_info.custom7.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom7_setting(ctx);

	return ERROR_NONE;
}	/* custom7 */

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
	S5HM6_LOG_INF("scenario_id = %d\n", scenario_id);

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
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM6] =
		imgsensor_info.custom6_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM7] =
		imgsensor_info.custom7_delay_frame;

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
#if ENABLE_PDAF
	sensor_info->PDAF_Support = 2;
#endif
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
	S5HM6_LOG_INF("scenario_id = %d\n", scenario_id);
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {

	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
		//imgsensor.sensor_mode = scenario_id;
		custom8_15(ctx, image_window, sensor_config_data);
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
	case SENSOR_SCENARIO_ID_CUSTOM7:
		custom7(ctx, image_window, sensor_config_data);
		break;
	default:
		S5HM6_LOG_INF("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control(ctx) */



static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	S5HM6_LOG_INF("framerate = %d\n ", framerate);
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
	set_max_framerate(ctx, ctx->current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx,
		kal_bool enable, UINT16 framerate)
{
	if (enable) /*enable auto flicker*/ {
		//ctx->autoflicker_en = KAL_TRUE;
		S5HM6_LOG_DBG("enable! fps = %d", framerate);
	} else {
		 /*Cancel Auto flick*/
		ctx->autoflicker_en = KAL_FALSE;
	}

	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	S5HM6_LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
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
		S5HM6_LOG_INF(
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
	case SENSOR_SCENARIO_ID_CUSTOM7:
		frame_length = imgsensor_info.custom7.pclk / framerate * 10
				/ imgsensor_info.custom7.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom7.framelength)
		? (frame_length - imgsensor_info.custom7.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom7.framelength
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
		S5HM6_LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	S5HM6_LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
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
	case SENSOR_SCENARIO_ID_CUSTOM7:
		*framerate = imgsensor_info.custom7.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_uint32 mode)
{
	S5HM6_LOG_INF("enable: %d\n", mode);
	//switch test pattern mode 
	switch (mode) {
	case SENSOR_TEST_PATTERN_MODE_BLACK:
		//All pixel data is replaced by 0% intensity (black) values.
		write_cmos_sensor(ctx, 0x0600, 0x0001); /*black mode*/
		write_cmos_sensor(ctx, 0x0602, 0x0000); /*black mode data*/
		write_cmos_sensor(ctx, 0x0604, 0x0000); /*black mode data*/
		write_cmos_sensor(ctx, 0x0606, 0x0000); /*black mode data*/
		write_cmos_sensor(ctx, 0x0608, 0x0000); /*black mode data*/
		break;
	default:
		write_cmos_sensor(ctx, 0x0600, 0x0000);	/*No pattern or other mode*/
		break;
	}
	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	UINT32 val                 = 0;
	UINT32 temperature         = 0;
	UINT32 temperature_convert = 0;

	/*enable temperature TEMP_SEN_CTL */
	val = read_cmos_sensor(ctx, 0x000F);
	S5HM6_LOG_INF("read 000F ==> 0x%x\n", val);

	temperature = read_cmos_sensor(ctx, 0x0020);
	temperature_convert = temperature >> 8;

	S5HM6_LOG_INF("temp_c(%d), read_reg(%d)\n", temperature_convert, temperature);
	return temperature_convert;
}

#if SEAMLESS_SWITCH_ENABLE
static kal_uint32 seamless_switch(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, uint32_t *ae_ctrl)
{
	uint32_t gain	 = ae_ctrl[5];
	uint32_t shutter = ae_ctrl[0];

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		S5HM6_LOG_INF("seamless switch to preview!\n");
		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.pre.pclk;
		ctx->line_length = imgsensor_info.pre.linelength;
		ctx->frame_length = imgsensor_info.pre.framelength;
		ctx->min_frame_length = imgsensor_info.pre.framelength;

		write_cmos_sensor(ctx, 0xFCFC, 0x4000);
		write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
		write_cmos_sensor(ctx, 0x0B30, 0x0100);
		if (gain != 0) {
			set_gain(ctx, gain);
		}
		if (shutter != 0) {
			set_shutter(ctx, shutter);
		}
		write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);
		//mdelay(40);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		S5HM6_LOG_INF("seamless switch to 3x mode!\n");
		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.custom5.pclk;
		ctx->line_length = imgsensor_info.custom5.linelength;
		ctx->frame_length = imgsensor_info.custom5.framelength;
		ctx->min_frame_length = imgsensor_info.custom5.framelength;

		write_cmos_sensor(ctx, 0xFCFC, 0x4000);
		write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0101);
		write_cmos_sensor(ctx, 0x0B30, 0x0101);
		if (gain != 0) {
			set_gain(ctx, gain);
		}
		if (shutter != 0) {
			set_shutter(ctx, shutter);
		}
		write_cmos_sensor(ctx, GROUP_HOLD_ADDR, 0x0001);
		//mdelay(40);
		break;
	default:
		S5HM6_LOG_INF(
		"error! wrong setting in set_seamless_switch = %d",
		scenario_id);
		return 0xff;
	}

	ctx->fast_mode_on = KAL_TRUE;
	S5HM6_LOG_INF("%s success, scenario is switched to %d", __func__, scenario_id);
	return 0;
}
#endif

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
#if ENABLE_PDAF
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif
#if SEAMLESS_SWITCH_ENABLE
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB = NULL;
	UINT32 *pAeCtrls;
	UINT32 *pScenarios;
#endif
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*S5HM6_LOG_DBG("feature_id = %d\n", feature_id);*/
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
		case SENSOR_SCENARIO_ID_CUSTOM3:
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
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
			SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gb;
			break;
		}
		S5HM6_LOG_INF("SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO get:%d\n",*(feature_data + 1));
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
				sizeof(platos5khm6_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)platos5khm6_ana_gain_table,
			sizeof(platos5khm6_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(feature_data + 1) = imgsensor_info.min_gain;
			*(feature_data + 2) = 8 * BASEGAIN;
			break;
		default:
			*(feature_data + 1) = imgsensor_info.min_gain;
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
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom7.pclk;
			break;
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
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom7.framelength << 16)
				+ imgsensor_info.custom7.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
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
		set_gain(ctx, (UINT32) *feature_data);
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
		S5HM6_LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (UINT32)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		S5HM6_LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		S5HM6_LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
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
		case SENSOR_SCENARIO_ID_CUSTOM7:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[11],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
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
#if ENABLE_PDAF
	case SENSOR_FEATURE_GET_PDAF_INFO:
		S5HM6_LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
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
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM6:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[0], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:  //4000*2252
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM7:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[1], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[2], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[3], sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		S5HM6_LOG_INF(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		S5HM6_LOG_INF("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		platos5khm6_get_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		S5HM6_LOG_INF("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		platos5khm6_set_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		S5HM6_LOG_INF("PDAF mode :%d\n", *feature_data_16);
		ctx->pdaf_mode = *feature_data_16;
		break;
#endif

#if SEAMLESS_SWITCH_ENABLE
	case XIAOMI_FEATURE_ENABLE_SEAMLESS_SWITCH:
		enable_seamless = *feature_data_32;
		S5HM6_LOG_INF("enable seamless switch setting : %d", enable_seamless);
		break;
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
		if ((feature_data + 1) != NULL) {
			pAeCtrls =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			S5HM6_LOG_INF("warning! no ae_ctrl input");
		}

		if ((feature_data + 3) != NULL) {
			pSetSensorAWB =
			(struct SET_SENSOR_AWB_GAIN *)((uintptr_t)(*(feature_data + 3)));
		} else {
			S5HM6_LOG_INF("warning! no awb gain input");
		}

		if (feature_data == NULL) {
			S5HM6_LOG_INF("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}

		if (pSetSensorAWB != NULL) {
			platos5khm6_awb_gain(ctx, pSetSensorAWB);
			S5HM6_LOG_INF("update awb gain by seamless switch");
		}
		seamless_switch(ctx, (*feature_data), pAeCtrls);
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		if ((feature_data + 1) != NULL) {
			pScenarios =
			    (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			S5HM6_LOG_INF("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			*pScenarios = SENSOR_SCENARIO_ID_CUSTOM5;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
			break;
		default:
			*pScenarios = 0xff;
			break;
		}
		S5HM6_LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
			*feature_data, *pScenarios);
		break;
#endif

	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		S5HM6_LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
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
		S5HM6_LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		S5HM6_LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		S5HM6_LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*feature_return_para_32 = 1000; /*BINNING_NONE*/
			break;
		default:
			*feature_return_para_32 = 1315; /*BINNING_AVERAGED*/
			break;
		}
		S5HM6_LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
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
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom7.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
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
#if ENABLE_PDAF
	case SENSOR_FEATURE_GET_VC_INFO:
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM7:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
	break;
#endif
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to remosaic */
		if (ctx->sensor_mode == SENSOR_SCENARIO_ID_CUSTOM4 ||
			ctx->sensor_mode == SENSOR_SCENARIO_ID_CUSTOM5) {
			platos5khm6_awb_gain(ctx,
				(struct SET_SENSOR_AWB_GAIN *) feature_para);
		}
		if (feature_para) {
			last_sensor_awb = *((struct SET_SENSOR_AWB_GAIN *) feature_para);
		}
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
		kal_uint8 index =
			*(((kal_uint8 *)feature_para) + (*feature_para_len));

		platos5khm6_set_lsc_reg_setting(ctx, index, feature_data_16,
					  (*feature_para_len)/sizeof(UINT16));
		}
		break;
	case SENSOR_FEATURE_SET_FRAMELENGTH:
		set_frame_length(ctx, (UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME:
		set_multi_shutter_frame_length(ctx, (UINT16 *)(*feature_data),
				(UINT16) (*(feature_data + 1)),
				(UINT16) (*(feature_data + 2)));
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
			.hsize = 4000,
			.vsize = 3000,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2000,
			.vsize = 1500,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 2252,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2000,
			.vsize = 1126,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1920,
			.vsize = 1080,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 960,
			.vsize = 540,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 12000,
			.vsize = 9000,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 3000,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 1332,
			.vsize = 1000,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
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
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM6:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM7:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_vid);
		memcpy(fd->entry, frame_desc_vid, sizeof(frame_desc_vid));
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM2:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust2);
		memcpy(fd->entry, frame_desc_cust2, sizeof(frame_desc_cust2));
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
	default:
		return -1;
	}

	return 0;
}
#endif


static const struct subdrv_ctx defctx = {

	.ana_gain_def = 4 * BASEGAIN,
	.ana_gain_max = 40 * BASEGAIN,
	.ana_gain_min = 1 * BASEGAIN,
	.ana_gain_step = 1,
	.exposure_def = 0x3D0,
	/* support long exposure at most 128 times) */
	.exposure_max = 0x36626A - 24,
	.exposure_min = 6,
	.exposure_step = 2,
	.frame_time_delay_frame = 2,
	.margin = 24,
	.max_frame_length = 0x36626A,

	.mirror = IMAGE_HV_MIRROR,	/* mirrorflip information */
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
	.test_pattern = KAL_FALSE,
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20, /* record current sensor's i2c write id */
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int get_temp(struct subdrv_ctx *ctx, int *temp)
{
#if 0
	*temp = get_sensor_temperature(ctx) * 1000;
#endif
	return 0;
}

static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	csi_param->legacy_phy = 0;
	csi_param->not_fixed_trail_settle = 0;

	return 0;
}

static struct subdrv_ops ops = {
	.get_id          = get_imgsensor_id,
	.init_ctx        = init_ctx,
	.open            = open,
	.get_info        = get_info,
	.get_resolution  = get_resolution,
	.control         = control,
	.feature_control = feature_control,
	.close           = close,
	.get_csi_param = get_csi_param,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc  = get_frame_desc,
#endif
	.get_temp        = get_temp,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_AFVDD, 2800000, 3},
	{HW_ID_RST,   0,       1},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_AVDD,  1,       1},
	{HW_ID_DVDD,  1,       1},
	{HW_ID_RST,   1,       1},
	{HW_ID_MCLK,  24,      1},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 20},
};

const struct subdrv_entry platos5khm6_mipi_raw_entry = {
	.name = "platos5khm6_mipi_raw",
	.id = PLATOS5KHM6_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

