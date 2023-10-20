// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 YUECHUS5KGD2mipi_Sensor.c
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
#define PFX "YUECHUS5KGD2_camera_sensor"


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

#include "yuechus5kgd2_ana_gain_table.h"
#include "yuechus5kgd2mipiraw_Sensor.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define yuechus5kgd2_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u16(__VA_ARGS__)
#define yuechus5kgd2_burst_write_cmos_sensor(...) subdrv_i2c_wr_regs_u16_burst_for_addr_same(__VA_ARGS__)
#define LOG_TAG "[yuechus5kgd2]"
#define S5KGD2_LOG_INF(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)
#define S5KGD2_LOG_DBG(format, args...) pr_debug(LOG_TAG "[%s] " format, __func__, ##args)

#undef VENDOR_EDIT

#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */

#define SEAMLESS_SWITCH_ENABLE 0
#define EEPROM_SLAVE_ID        0xA0
// common register
#define SENSOR_ID_ADDR                0x0000
#define FRAME_LENGTH_LINES_ADDR       0x0340
#define FRAME_LENGTH_LINES_SHIFT_ADDR 0x0702
#define LINE_LENGTH_PIXEL_ADDR        0x0342
#define COARSE_INTEGRATION_TIME_ADDR  0x0202
#define COARSE_INTEGRATION_TIME_SHIFT_ADDR 0x0704
#define AGAIN_ADDR      0x0204
#define DGAIN_ADDR      0x020E
//#define GROUP_HOLD_ADDR 0x0104
#define AWB_R_GAIN_ADDR 0x0D82
#define AWB_G_GAIN_ADDR 0x0D84
#define AWB_B_GAIN_ADDR 0x0D86

static int set_gain_num = 0 ;
#define ENABLE_PDAF 1
static unsigned long long last_sof_timestamps_ns = 0;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = YUECHUS5KGD2_SENSOR_ID,

	.checksum_value = 0xa4c32546,

	.pre = {
		.pclk = 800000000,
		.linelength  = 6640,
		.framelength = 3996,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000,
	},
	.cap = { /*same preview*/
		.pclk = 800000000,
		.linelength  = 6640,
		.framelength = 3996,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000,
	},
	.normal_video = {
		.pclk = 800000000,
		.linelength  = 13000,
		.framelength = 2036,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 1848,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000,
	},
	.hs_video = {
		.pclk = 1600000000,
		.linelength  = 7856,
		.framelength = 1694,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1632,
		.grabwindow_height = 928,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 576000000,
	},
	.slim_video = {/*same preview*/
		.pclk = 800000000,
		.linelength  = 6640,
		.framelength = 3996,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000,
	},
	.custom1 = {/*video hdr*/
		.pclk = 800000000,
		.linelength  = 6304,
		.framelength = 2108,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 1848,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 576000000,
	},
	.custom2 = {/*same as hs_video*/
		.pclk = 1600000000,
		.linelength  = 7856,
		.framelength = 1694,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1632,
		.grabwindow_height = 928,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 576000000,
	},
	.custom3 = {//bokeh
		.pclk = 800000000,
		.linelength  = 6640,
		.framelength = 4996,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
		.mipi_pixel_rate = 576000000,
	},
	.margin       = 10, 				/* sensor framelength & shutter margin */
	.min_shutter  = 12, 				/* min shutter */
	.min_gain     = BASEGAIN, 		//1x
	.max_gain     = 16 * BASEGAIN,	//40x
	.min_gain_iso = 50,
	.exp_step     = 1,
	.gain_step    = 1,
	.gain_type    = 0,

	.max_frame_length           = 0xffff,
	.ae_shut_delay_frame        = 3,
	.ae_sensor_gain_delay_frame = 3,
	.ae_ispGain_delay_frame     = 3, /* isp gain delay frame for AE cycle */
	.frame_time_delay_frame     = 2,

	.ihdr_support               = 0, /* 1, support; 0,not support */
	.ihdr_le_firstline          = 0, /* 1,le first ; 0, se first */
	.temperature_support        = 0, /* 1, support; 0,not support */

	.sensor_mode_num        = 8, /* support sensor mode num */
	.cap_delay_frame        = 2,
	.pre_delay_frame        = 2, /* enter preview delay frame num */
	.video_delay_frame      = 2, /* enter video delay frame num */
	.hs_video_delay_frame   = 2, /* enter hs_video delay frame num */
	.slim_video_delay_frame = 2, /* enter slim_video delay frame num */
	.custom1_delay_frame    = 2, /* enter custom1 delay frame num */
	.custom2_delay_frame    = 2, /* enter custom2 delay frame num */
	.custom3_delay_frame	= 2, /* enter custom3 delay frame num */

	.isp_driving_current      = ISP_DRIVING_6MA,
	.sensor_interface_type    = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type         = MIPI_OPHY_NCSI2, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode   = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num            = SENSOR_MIPI_4_LANE,
	.i2c_addr_table           = {0x20, 0xff},
	.i2c_speed                = 1000, /* i2c read/write speed */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] = {
	{ 6560, 4928,   0,   0, 6560, 4928, 3280, 2464,    0,    0, 3280, 2464, 0, 0, 3280, 2464}, // Preview
	{ 6560, 4928,   0,   0, 6560, 4928, 3280, 2464,    0,    0, 3280, 2464, 0, 0, 3280, 2464}, // capture
	{ 6560, 4928,   0, 612, 6560, 3704, 3280, 1852,    0,    2, 3280, 1848, 0, 0, 3280, 1848}, // video
	{ 6560, 4928,  16, 608, 6528, 3712, 1632,  928,    0,    0, 1632,  928, 0, 0, 1632,  928}, // hs_video as Preview
	{ 6560, 4928,   0,   0, 6560, 4928, 3280, 2464,    0,    0, 3280, 2464, 0, 0, 3280, 2464}, // slim_video as Preview
	{ 6560, 4928,   0, 612, 6560, 3704, 3280, 1852,    0,    2, 3280, 1848, 0, 0, 3280, 1848}, // custom1 video hdr
	{ 6560, 4928,  16, 608, 6528, 3712, 1632,  928,    0,    0, 1632,  928, 0, 0, 1632,  928}, // custom2 as hs_video
	{ 6560, 4928,   0,   0, 6560, 4928, 3280, 2464,    0,    0, 3280, 2464, 0, 0, 3280, 2464}, // custom3 as Preview
};

#if ENABLE_PDAF
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[] = {
	{/* preview/capture/slim_video/cust3 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 3280, 2464, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 400, 2448, 0x00, 0x00, 0x0000, 0x0000
	},
	{/* normal video / cust1 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 3280, 1848, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 400, 1840, 0x00, 0x00, 0x0000, 0x0000
	},
	{/* hs_video / cust2*/
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 1632, 928, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 416, 460, 0x00, 0x00, 0x0000, 0x0000
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info[3] = {
	{ //pre
		.i4OffsetX	= 40,
		.i4OffsetY	= 8,
		.i4PitchX	= 16,
		.i4PitchY	= 16,
		.i4PairNum	= 16,
		.i4SubBlkW	= 8,
		.i4SubBlkH	= 2,
		.i4BlockNumX = 200,
		.i4BlockNumY = 153,
		.iMirrorFlip = IMAGE_HV_MIRROR,

		.i4PosL = {
			{41, 8}, {49, 8}, {43, 11},{51, 11}, {47, 12}, {55, 12}, {45, 15}, {53, 15},
			{41, 16}, {49, 16}, {43, 19}, {51, 19}, {47, 20}, {55, 20}, {45, 23}, {53, 23},
		},
		.i4PosR = {
			{40, 8}, {48, 8}, {42, 11},{50, 11}, {46, 12}, {54, 12}, {44, 15}, {52, 15},
			{40, 16}, {48, 16}, {42, 19}, {50, 19}, {46, 20}, {54, 20}, {44, 23}, {52, 23},
		},
	},
	{// video
		.i4OffsetX	= 40,
		.i4OffsetY	= 8,
		.i4PitchX	= 16,
		.i4PitchY	= 16,
		.i4PairNum	= 16,
		.i4SubBlkW	= 8,
		.i4SubBlkH	= 2,
		.i4BlockNumX = 200,
		.i4BlockNumY = 115,
		.iMirrorFlip = IMAGE_HV_MIRROR,

		.i4PosL = {
			{41, 8}, {49, 8}, {43, 11},{51, 11}, {47, 12}, {55, 12}, {45, 15}, {53, 15},
			{41, 16}, {49, 16}, {43, 19}, {51, 19}, {47, 20}, {55, 20}, {45, 23}, {53, 23},
		},
		.i4PosR = {
			{40, 8}, {48, 8}, {42, 11},{50, 11}, {46, 12}, {54, 12}, {44, 15}, {52, 15},
			{40, 16}, {48, 16}, {42, 19}, {50, 19}, {46, 20}, {54, 20}, {44, 23}, {52, 23},
		},

		.i4Crop = {
			{0,0},{0,0},{0,308},{0,0},{0,0},{0,308},{0,0},{0,0},
		},

	},
	{//hs_video
		.i4OffsetX	= 4,
		.i4OffsetY	= 4,
		.i4PitchX	= 8,
		.i4PitchY	= 8,
		.i4PairNum	= 4,
		.i4SubBlkW	= 4,
		.i4SubBlkH	= 4,
		.i4BlockNumX = 204,
		.i4BlockNumY = 114,
		.iMirrorFlip = IMAGE_HV_MIRROR,
		.i4PosL = {
			{5, 4}, {9, 4}, {7, 8},{11, 8},
		},
		.i4PosR = {
			{4, 4}, {8, 4}, {6, 8},{10, 8},
		},

		.i4Crop = {
			{0,0},{0,0},{0,0},{4,152}, {0,0},{0,0},{4,152},{0,0},
		},

	},
};


static void yuechus5kgd2_get_pdaf_reg_setting(struct subdrv_ctx *ctx, MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(ctx, regDa[idx]);
		S5KGD2_LOG_DBG("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void yuechus5kgd2_set_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor_8(ctx, regDa[idx], regDa[idx + 1]);
		S5KGD2_LOG_DBG("%x %x", regDa[idx], regDa[idx+1]);
	}
}
#endif


static void set_dummy(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_DBG("dummyline = %d, dummypixels = %d \n", ctx->dummy_line, ctx->dummy_pixel);

	write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
	write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0342, ctx->line_length >> 8);
	write_cmos_sensor_8(ctx, 0x0343, ctx->line_length & 0xFF);
}	/*	set_dummy  */


static void set_max_framerate(struct subdrv_ctx *ctx,
		UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = ctx->frame_length;

	S5KGD2_LOG_INF(
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

static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

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

	if (ctx->autoflicker_en == KAL_TRUE) {
		realtime_fps = ctx->pclk / ctx->line_length * 10
				/ ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
				set_max_framerate(ctx, 296, 0);
				//set_dummy();
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
				set_max_framerate(ctx, 146, 0);
				//set_dummy();
		} else {
				write_cmos_sensor_8(ctx,0x0340, ctx->frame_length >> 8);
				write_cmos_sensor_8(ctx,0x0341, ctx->frame_length & 0xFF);
		}
	}
	// Update Shutter
	write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
	write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, shutter & 0xFF);
	S5KGD2_LOG_DBG("realtime_fps =%d\n", realtime_fps);
	S5KGD2_LOG_DBG("shutter =%d, framelength =%d\n",
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
	write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, ctx->frame_length & 0xFFFF);

	S5KGD2_LOG_INF("Framelength: set=%d/input=%d/min=%d, auto_extend=%d\n",
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
			write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, ctx->frame_length & 0xFFFF);
		}
	}

	if (shutter & 0xFFFF0000) {
		Rshift = 6;
	}
	else {
		Rshift = 0;
	}

	// Update Shutter
	write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
	write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, shutter & 0xFF);
	S5KGD2_LOG_DBG("set shutter =%d, framelength =%d\n", shutter, ctx->frame_length);

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

		write_cmos_sensor(ctx, FRAME_LENGTH_LINES_ADDR, ctx->frame_length);
		write_cmos_sensor(ctx, COARSE_INTEGRATION_TIME_ADDR, shutters[0]);

		S5KGD2_LOG_DBG("shutter =%d, framelength =%d\n",	shutters[0], ctx->frame_length);
	}
}

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint16 gain)
{
	return (kal_uint16) gain / 32;//1X:gain=1024, gain= gain *32 / 1024 = gain / 32
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
static kal_uint16 set_gain(struct subdrv_ctx *ctx, kal_uint16 gain)
{
	kal_uint16 reg_gain, max_gain, min_gain;

	max_gain = imgsensor_info.max_gain;//setuphere for mode use
	min_gain = imgsensor_info.min_gain;//setuphere for mode use

	if (ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM4 ||//16x for full mode
			ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM5) {
		max_gain = 16 * BASEGAIN;
	}

	if (gain < min_gain || gain > max_gain) {
		S5KGD2_LOG_INF("Error max gain setting: %d Should between %d & %d\n",
			gain, min_gain, max_gain);
		if (gain < min_gain)
			gain = min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;

	S5KGD2_LOG_DBG("ctx->sensor_mode: %d",ctx->sensor_mode);
	S5KGD2_LOG_DBG("gain = %d, ctx->gain = 0x%x ,reg_gain = 0x%x, max_gain:0x%x\n ",
		gain, ctx->gain, reg_gain, max_gain);
	if(reg_gain){
		//write_cmos_sensor(0x0204,reg_gain);
		write_cmos_sensor_8(ctx, 0x0204, (reg_gain >> 8));
		write_cmos_sensor_8(ctx, 0x0205, (reg_gain & 0xff));
		//write_cmos_sensor(ctx, AGAIN_ADDR, reg_gain);
	}

	return gain;
} /* set_gain */

static kal_uint32 yuechus5kgd2_awb_gain(struct subdrv_ctx *ctx,
		struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	return ERROR_NONE;
}


static void yuechus5kgd2_set_lsc_reg_setting(struct subdrv_ctx *ctx,
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

	S5KGD2_LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
			write_cmos_sensor(ctx, 0x6028, 0x4000);
			mDELAY(5);
			write_cmos_sensor(ctx, 0x0100, 0x0103);
			mDELAY(5);
	} else {
			write_cmos_sensor(ctx, 0x6028, 0x4000);
			write_cmos_sensor(ctx, 0x0100, 0x0000);	
	}
	return ERROR_NONE;
}

static kal_uint16 yuechus5kgd2_fast_write_cmos_sensor(struct subdrv_ctx *ctx,
								 kal_uint16 *para, kal_uint32 len)
{
	kal_uint32 burst_thr     = 100;
	kal_uint32 IDX           = 0;
	kal_uint16 addr          = 0;
	kal_uint32 table_pos_sta = 0;
	kal_uint32 table_len     = 0;
	kal_uint32 burst_pos_sta = 0;
	kal_uint32 burst_len     = 0;
	kal_uint32 need_write    = 0;

	S5KGD2_LOG_INF("len = %d +\n", len);

	while (len > IDX) {
		addr = para[IDX];
		if (addr == 0x6F12) {
			if (burst_len == 0) {
				burst_pos_sta = IDX;
			}
			burst_len += 2;
			} else {
				if (table_len == 0) {
					table_pos_sta = IDX;
				}
				table_len += 2;
		}
		if (len == (IDX + 2)) {
			/* the end */
			need_write = 1;
		} else {
			addr = para[IDX + 2];
			if (addr != 0x6F12) {
				if (burst_len > burst_thr) {
					need_write = 1;
				} else if (burst_len > 0) {
					table_len += burst_len;
					burst_len = 0;
				}
			}
		}
		if (need_write) {
			if (table_len > 0) {
				/* Find the sequence that requires Table Mode */
				yuechus5kgd2_table_write_cmos_sensor(ctx, para + table_pos_sta, table_len);
				S5KGD2_LOG_INF("table_pos_sta = %d, table_len = %d\n", table_pos_sta, table_len);
				table_len = 0;
			}
			if (burst_len > 0) {
				/* Find the sequence that requires Burst Mode */
				yuechus5kgd2_burst_write_cmos_sensor(ctx, para + burst_pos_sta, burst_len);
				S5KGD2_LOG_INF("burst_pos_sta = %d, burst_len = %d\n", burst_pos_sta, burst_len);
				burst_len = 0;
			}
			need_write = 0;
		}
		IDX += 2;
	}
	S5KGD2_LOG_INF("len = %d -\n", len);
	return 0;
}

static void sensor_init(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_INF("+\n");
	write_cmos_sensor(ctx, 0xFCFC, 0x4000);
	write_cmos_sensor(ctx, 0x6028, 0x4000);
	write_cmos_sensor(ctx, 0x0000, 0x0026);
	write_cmos_sensor(ctx, 0x0000, 0x0842);
	write_cmos_sensor(ctx, 0x6010, 0x0001);
	mdelay(30);
	write_cmos_sensor(ctx, 0x6214, 0xFF7D);
	write_cmos_sensor(ctx, 0x6218, 0x0000);
	write_cmos_sensor(ctx, 0x0A02, 0xFFFF);
	yuechus5kgd2_fast_write_cmos_sensor(ctx, yuechus5kgd2_init_setting,
		sizeof(yuechus5kgd2_init_setting) / sizeof(kal_uint16));

	S5KGD2_LOG_INF("-\n");
}	/*	  sensor_init  */

static void preview_setting(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_INF("+\n");
	
	yuechus5kgd2_table_write_cmos_sensor(ctx, yuechus5kgd2_preview_setting,
			sizeof(yuechus5kgd2_preview_setting) / sizeof(kal_uint16));

	S5KGD2_LOG_INF("-\n");
} /* preview_setting */

static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	S5KGD2_LOG_INF("+\n");
	preview_setting(ctx);
	S5KGD2_LOG_INF("-\n");
}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	S5KGD2_LOG_INF("+\n");
	yuechus5kgd2_table_write_cmos_sensor(ctx, yuechus5kgd2_normal_video_setting,
		sizeof(yuechus5kgd2_normal_video_setting)/sizeof(kal_uint16));
	S5KGD2_LOG_INF("-\n");
}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_INF("+\n");
	yuechus5kgd2_table_write_cmos_sensor(ctx, yuechus5kgd2_custom2_setting,
				sizeof(yuechus5kgd2_custom2_setting) / sizeof(kal_uint16));
	S5KGD2_LOG_INF("-\n");
}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_INF("+\n");
	preview_setting(ctx);
	S5KGD2_LOG_INF("-\n");
}

static void custom1_setting(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_INF("+\n");
	yuechus5kgd2_table_write_cmos_sensor(ctx, yuechus5kgd2_custom1_setting,
			sizeof(yuechus5kgd2_custom1_setting) / sizeof(kal_uint16));
	S5KGD2_LOG_INF("-\n");
}

static void custom2_setting(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_INF("+\n");
	yuechus5kgd2_table_write_cmos_sensor(ctx, yuechus5kgd2_custom2_setting,
			sizeof(yuechus5kgd2_custom2_setting) / sizeof(kal_uint16));
	S5KGD2_LOG_INF("-\n");
}

static void custom3_setting(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_INF("+\n");
	preview_setting(ctx);
	S5KGD2_LOG_INF("-\n");
}

static kal_uint32 return_sensor_id(struct subdrv_ctx *ctx)
{
	kal_uint32 sensor_id = 0;

	sensor_id = ((read_cmos_sensor_8(ctx, SENSOR_ID_ADDR) << 8) | read_cmos_sensor_8(ctx, SENSOR_ID_ADDR + 1));

	S5KGD2_LOG_INF("[%s] sensor_id: 0x%x", __func__, sensor_id);

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
				S5KGD2_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			S5KGD2_LOG_INF("Read sensor id fail, id: 0x%x\n",
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

	S5KGD2_LOG_INF("+\n");
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = return_sensor_id(ctx);

			if (sensor_id == imgsensor_info.sensor_id) {
				S5KGD2_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			S5KGD2_LOG_INF("Read sensor id fail, id: 0x%x\n",
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
	set_gain_num = 0;

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
	S5KGD2_LOG_INF("-\n");

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
	S5KGD2_LOG_INF("E\n");
	/* No Need to implement this function */
	streaming_control(ctx, KAL_FALSE);

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
	S5KGD2_LOG_INF("E\n");

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
	S5KGD2_LOG_INF("E\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		S5KGD2_LOG_INF(
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
	S5KGD2_LOG_INF("E\n");

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
	S5KGD2_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;
	hs_video_setting(ctx);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KGD2_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;
	slim_video_setting(ctx);

	return ERROR_NONE;
}	/* slim_video */


static kal_uint32 custom1(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KGD2_LOG_INF("E\n");

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
	S5KGD2_LOG_INF("E\n");

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
	S5KGD2_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom3.pclk;
	ctx->line_length = imgsensor_info.custom3.linelength;
	ctx->frame_length = imgsensor_info.custom3.framelength;
	ctx->min_frame_length = imgsensor_info.custom3.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom3_setting(ctx);
	return ERROR_NONE;
}	/* custom2 */

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
	S5KGD2_LOG_INF("scenario_id = %d\n", scenario_id);

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
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
#if ENABLE_PDAF
		sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
#endif
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
	S5KGD2_LOG_INF("scenario_id = %d\n", scenario_id);
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
		S5KGD2_LOG_INF("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control(ctx) */



static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	S5KGD2_LOG_INF("framerate = %d\n ", framerate);
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
		S5KGD2_LOG_DBG("enable! fps = %d", framerate);
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

	S5KGD2_LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

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
		S5KGD2_LOG_INF(
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
		? (frame_length - imgsensor_info.hs_video.framelength) : 0;
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
		? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.hs_video.framelength
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
			? (frame_length - imgsensor_info.custom3.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom3.framelength
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
		S5KGD2_LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	S5KGD2_LOG_INF("scenario_id = %d\n", scenario_id);

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
	S5KGD2_LOG_INF("+");
	return ERROR_NONE;
#if 0
	S5KGD2_LOG_INF("enable: %d\n", enable);

	if (enable) {
		// 0x5081[0]: 1 enable,  0 disable
		// 0x5081[5:4]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor_8(ctx, 0x601, 0x02);
	} else {
		write_cmos_sensor_8(ctx, 0x0601, 0x00);
	}

	write_cmos_sensor_8(ctx, 0x3200, 0x00);
	ctx->test_pattern = enable;

	return ERROR_NONE;
#endif
}

static kal_uint32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	S5KGD2_LOG_INF("+");
	return ERROR_NONE;
#if 0
	UINT8 temperature = 0, temp;
	static kal_uint32 temp_c;
	UINT32 temperature_convert = 0;

	/*enable temperature TEMP_SEN_CTL */
	temp = read_cmos_sensor(ctx, 0x4d12);
	write_cmos_sensor(ctx, 0x4d12, temp | 0x01);
	temperature = read_cmos_sensor(ctx, 0x4d13);

	if (temperature > 0 && temperature < 85) {
		temperature_convert = temperature;
		temp_c = temperature_convert;
	} else {
		temperature_convert = temp_c;//using last normal temperature
	}

	S5KGD2_LOG_INF("temp_c(%d), read_reg(%d)\n", temperature_convert, temperature);
	return temperature_convert;
#endif
}

static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	kal_uint16 sensor_output_cnt;

	sensor_output_cnt = read_cmos_sensor_8(ctx, 0x0005);
	last_sof_timestamps_ns = ktime_get_boottime_ns();

	S5KGD2_LOG_INF("sensormode(%d) sof_cnt(%d) sensor_output_cnt(%d)\n",
		ctx->sensor_mode, sof_cnt, sensor_output_cnt);
	return 0;
};


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


	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	//S5KGD2_LOG_INF("feature_id = %d\n", feature_id);

#if ENABLE_PDAF
		struct SET_PD_BLOCK_INFO_T *PDAFinfo;
		struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif

	switch (feature_id) {
	case SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO:
		switch (*feature_data) {
		default:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		}
		S5KGD2_LOG_INF("SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO get:%d\n",*(feature_data + 1));
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
				sizeof(yuechus5kgd2_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)yuechus5kgd2_ana_gain_table,
			sizeof(yuechus5kgd2_ana_gain_table));
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
		case SENSOR_SCENARIO_ID_CUSTOM5:
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
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
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
		S5KGD2_LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
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
		S5KGD2_LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		S5KGD2_LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
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
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
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
#if ENABLE_PDAF
		case SENSOR_FEATURE_GET_PDAF_INFO:
			S5KGD2_LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
				(UINT16) *feature_data);
			PDAFinfo =
			  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
			switch (*feature_data) {
			case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			case SENSOR_SCENARIO_ID_CUSTOM3:
				memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[0], sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case SENSOR_SCENARIO_ID_NORMAL_VIDEO:  //4000*2256
			case SENSOR_SCENARIO_ID_CUSTOM1:
				memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[1], sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			case SENSOR_SCENARIO_ID_CUSTOM2:
				memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info[2], sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case SENSOR_SCENARIO_ID_CUSTOM4:
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
				break;
			}
			break;
		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
			S5KGD2_LOG_INF(
			"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
				(UINT16) *feature_data);
			switch (*feature_data) {
			case SENSOR_SCENARIO_ID_CUSTOM4:
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
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			case SENSOR_SCENARIO_ID_CUSTOM1:
			case SENSOR_SCENARIO_ID_CUSTOM2:
			case SENSOR_SCENARIO_ID_CUSTOM3:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			}
			break;
		case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
			S5KGD2_LOG_INF("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
				(*feature_para_len));
			yuechus5kgd2_get_pdaf_reg_setting(ctx,
					(*feature_para_len) / sizeof(UINT32),
					feature_data_16);
			break;
		case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
			S5KGD2_LOG_INF("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
				(*feature_para_len));
			yuechus5kgd2_set_pdaf_reg_setting(ctx,
					(*feature_para_len) / sizeof(UINT32),
					feature_data_16);
			break;
		case SENSOR_FEATURE_SET_PDAF:
			S5KGD2_LOG_INF("PDAF mode :%d\n", *feature_data_16);
			ctx->pdaf_mode = *feature_data_16;
			break;
#endif
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		S5KGD2_LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
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
		S5KGD2_LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		S5KGD2_LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		S5KGD2_LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		default:
			*feature_return_para_32 = 1000; /*BINNING_AVERAGED*/
			break;
		}
		S5KGD2_LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
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
#if ENABLE_PDAF
		case SENSOR_FEATURE_GET_VC_INFO:
			pvcinfo =
			 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
			switch (*feature_data_32) {
			case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			case SENSOR_SCENARIO_ID_CUSTOM1:
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case SENSOR_SCENARIO_ID_CUSTOM2:
			case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			case SENSOR_SCENARIO_ID_CUSTOM3:
			case SENSOR_SCENARIO_ID_CUSTOM4:
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
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			}
		break;
#endif
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to separate 3hdr and remosaic */
		yuechus5kgd2_awb_gain(ctx,
			(struct SET_SENSOR_AWB_GAIN *) feature_para);
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
		kal_uint8 index =
			*(((kal_uint8 *)feature_para) + (*feature_para_len));

		yuechus5kgd2_set_lsc_reg_setting(ctx, index, feature_data_16,
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
			.hsize = 3280,
			.vsize = 2464,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 400,
			.vsize = 2448,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},

};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3280,
			.vsize = 1848,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 400,
			.vsize = 1840,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},

};


static struct mtk_mbus_frame_desc_entry frame_desc_cust2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1632,
			.vsize = 928,
		},
	},

	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 416,
			.vsize = 460,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},

};


static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM4:
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
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
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
	default:
		return -1;
	}

	return 0;
}
#endif


static const struct subdrv_ctx defctx = {

	.ana_gain_def = 4 * BASEGAIN,
	.ana_gain_max = 16 * BASEGAIN,
	.ana_gain_min = 1 * BASEGAIN,
	.ana_gain_step = 1,
	.exposure_def = 0x3D0,
	.exposure_max = 0xFFFF - 10,
	.exposure_min = 12,
	.exposure_step = 2,
	.frame_time_delay_frame = 2,
	.margin = 10,
	.max_frame_length = 0xFFFF,

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
	*temp = get_sensor_temperature(ctx) * 1000;
	return 0;
}

static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	csi_param->legacy_phy = 0;
	csi_param->not_fixed_trail_settle = 0;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM2:
		csi_param->dphy_trail = 80;
		break;
	default:
		csi_param->dphy_trail = 80;
		break;
	}
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
	.get_csi_param 	= get_csi_param,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc  = get_frame_desc,
#endif
	.get_temp        = get_temp,
	.vsync_notify	 = vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST,   0,       1},
	{HW_ID_AVDD1, 1200000, 1}, //VCAM_LDO
	{HW_ID_AFVDD, 2800000, 1},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_DVDD,  1000000, 1},
	{HW_ID_AVDD,  1,       2},
	{HW_ID_RST,   1,       10},
	{HW_ID_MCLK,  24,      0},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 30},
};

const struct subdrv_entry yuechus5kgd2_mipi_raw_entry = {
	.name = "yuechus5kgd2_mipi_raw",
	.id = YUECHUS5KGD2_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

