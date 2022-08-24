// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *     OV13B10LZmipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 * Setting version:
 * ------------
 *   update full pd setting for OV13B10LZ
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#define PFX "ov13b10lz_camera_sensor"
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
#include "ov13b10lzmipiraw_Sensor.h"
#include "ov13b10lz_ana_gain_table.h"
#define cam_pr_debug(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)
#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor_16(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor_16(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define ov13b10lz_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)

#define MULTI_WRITE 1
#define _I2C_BUF_SIZE 4096
static kal_uint16 _i2c_data[_I2C_BUF_SIZE];
static unsigned int _size_to_write;
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV13B10LZ_SENSOR_ID,

	.checksum_value = 0x3acb7e3a,

	.pre = {
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3196,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 448000000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3196,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 448000000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 6392,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 448000000,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3196,
			.startx = 0,
			.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 448000000,
			.max_framerate = 300,

	},
	.hs_video = {
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 798,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 224000000,
		.max_framerate = 1200,

	},
		.slim_video = {
		.pclk = 112000000,
		.linelength =  1176,
		.framelength = 1588,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 224000000,
		.max_framerate = 600,

	},
    .custom1 = {
        .pclk = 112000000,
        .linelength = 1176,
        .framelength = 3196,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4208,
        .grabwindow_height = 3120,
        .mipi_data_lp2hs_settle_dc = 19,
        .mipi_pixel_rate = 448000000,
        .max_framerate = 300,
     },
	.margin = 0x8,					/* sensor framelength & shutter margin */
	.min_shutter = 0x4,				/* min shutter */
	.min_gain = BASEGAIN, /*1x gain*/
	.max_gain = 992, /*15.5x * 1024  gain*/
	.min_gain_iso = 100,
	.exp_step = 2,
	.gain_step = 1, /*minimum step = 4 in 1x~2x gain*/
	.gain_type = 3,/*to be modify,no gain table for sony*/
	.max_frame_length = 0x7fff,     /* max framelength by sensor register's limitation */
	.ae_shut_delay_frame = 0,		//check
	.ae_sensor_gain_delay_frame = 0,//check
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 6,
    .frame_time_delay_frame= 1,
	.cap_delay_frame = 3,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x6c, 0xff},
	.i2c_speed = 400,

};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	/* Preview check*/
    { 4224, 3136,    0,   0, 4224, 3136,   4224,  3136,   8,    8,  4208, 3120,    0,    0,   4208, 3120}, //PREVIEW
	/* capture */
    { 4224, 3136,    0,   0, 4224, 3136,   4224,  3136,   8,    8,  4208, 3120,    0,    0,   4208, 3120}, //CAPTURE
	/* video */
    { 4224, 3136,    0,   0, 4224, 3136,   4224,  3136,   8,    8,  4208, 3120,    0,    0,   4208, 3120}, //NORMAL VIDEO
	/* hs video */
    { 4224, 3136,    0,   0, 4224, 3136,   2112,  1568, 416,  424,  1280,  720,    0,    0,   1280,  720},// HS VIDEO
	/* slim video */
	{ 4224, 3136,    0,   0, 4224, 3136,   2112,  1568,  96,  244,  1920, 1080,    0,    0,   1920, 1080}, //SLIM VIDEO
	{ 4224, 3136,    0,   0, 4224, 3136,   4224,  3136,   8,    8,  4208, 3120,    0,    0,   4208, 3120}, //custom1 
};

/*PD information update*/
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 8,
	.i4OffsetY = 8,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum =  8,
	.i4SubBlkW = 16,
	.i4SubBlkH =  8,
	.i4PosL = {{22, 14}, {38, 14}, {14, 18}, {30, 18}, {22, 30}, {38, 30}, {14, 34}, {30, 34}},
	.i4PosR = {{22, 10}, {38, 10}, {14, 22}, {30, 22}, {22, 26}, {38, 26}, {14, 38}, {30, 38}},
	.iMirrorFlip = 0,
	.i4BlockNumX = 131,
	.i4BlockNumY = 97,
};

#if MULTI_WRITE
#define I2C_BUFFER_LEN 225	/*trans# max is 255, each 3 bytes*/
#else
#define I2C_BUFFER_LEN 3
#endif



#if 0
static kal_uint16 read_sensor_eeprom(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, OV13B10LZ_TS_EEPROM);

	return get_byte;
}
#endif

static void set_dummy(struct subdrv_ctx *ctx)
{
	write_cmos_sensor(ctx,0x380c, ctx->line_length >> 8);
	write_cmos_sensor(ctx,0x380d, ctx->line_length & 0xFF);
	write_cmos_sensor(ctx,0x380e, (ctx->frame_length >> 8) & 0x7f);
	write_cmos_sensor(ctx,0x380f, ctx->frame_length & 0xFF);
}

static void set_max_framerate(struct subdrv_ctx *ctx,UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = ctx->frame_length;

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;


	ctx->frame_length = (frame_length > ctx->min_frame_length) ?
			frame_length : ctx->min_frame_length;
	ctx->dummy_line = ctx->frame_length -
		ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;
		ctx->dummy_line = ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;


	set_dummy(ctx);
}

static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter) ?
				imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
				(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
				(imgsensor_info.max_frame_length - imgsensor_info.margin) :
				shutter;

	//frame_length and shutter should be an even number.
	//shutter = (shutter >> 1) << 1;
	//ctx->frame_length = (ctx->frame_length >> 1) << 1;

	if (ctx->autoflicker_en == KAL_TRUE) {
		realtime_fps = ctx->pclk / ctx->line_length * 10 /
			ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			realtime_fps = 296;
			set_max_framerate(ctx, realtime_fps, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			realtime_fps = 146;
			set_max_framerate(ctx,realtime_fps, 0);
		} else	{
			/* Extend frame length */
			write_cmos_sensor(ctx,0x380e,(ctx->frame_length >> 8) & 0x7f);
			write_cmos_sensor(ctx,0x380f, ctx->frame_length & 0xFF);
		}
	} else	{
		/* Extend frame length */
		write_cmos_sensor(ctx,0x380e, (ctx->frame_length >> 8) & 0x7f);
		write_cmos_sensor(ctx,0x380f, ctx->frame_length & 0xFF);
	}

	/* Update Shutter */
	write_cmos_sensor(ctx,0x3500, (shutter >> 16) & 0xFF);
	write_cmos_sensor(ctx,0x3501, (shutter >> 8) & 0xFF);
	write_cmos_sensor(ctx,0x3502, shutter & 0xFF);

	cam_pr_debug("shutter =%d, framelength =%d, realtime_fps =%d\n",
		shutter, ctx->frame_length, realtime_fps);
}
//should not be kal_uint16 -- can't reach long exp
static void set_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{

	ctx->shutter = shutter;
	write_shutter(ctx, shutter);
}

static void set_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint32 shutter, kal_uint16 frame_length)
{
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	ctx->shutter = shutter;

	pr_debug("ov13b10lz %s %d\n", __func__, __LINE__);


	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;
	ctx->frame_length = ctx->frame_length + dummy_line;

	if (shutter > ctx->frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;


	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk
			/ ctx->line_length * 10 / ctx->frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx,296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			write_cmos_sensor(ctx,0x380e, (ctx->frame_length >> 8) & 0x7f);
            write_cmos_sensor(ctx,0x380f, ctx->frame_length & 0xFF);
		}
	} else {
			write_cmos_sensor(ctx,0x380e, (ctx->frame_length >> 8) & 0x7f);
            write_cmos_sensor(ctx,0x380f, ctx->frame_length & 0xFF);
	}

	/* Update Shutter */
	write_cmos_sensor(ctx,0x3500, (shutter >> 16) & 0xFF);
	write_cmos_sensor(ctx,0x3501, (shutter >> 8) & 0xFF);
	write_cmos_sensor(ctx,0x3502, shutter & 0xFF);


}
static kal_uint16 gain2reg(struct subdrv_ctx *ctx,const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	/* platform 1xgain = 64, sensor driver 1*gain = 0x80 */
	iReg = gain*256 / BASEGAIN;

	/* sensor 1xGain */
	if (iReg < 0x100)
		iReg = 0x100;

	/* sensor 15.5xGain */
	if (iReg > 0xf80)
		iReg = 0xf80;

	return iReg;
}

static kal_uint16 set_gain(struct subdrv_ctx *ctx,kal_uint16 gain)
{
	kal_uint16 reg_gain;

	reg_gain = gain2reg(ctx,gain);

	ctx->gain = reg_gain;


	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	write_cmos_sensor(ctx,0x3508, (reg_gain >> 8));
	write_cmos_sensor(ctx,0x3509, (reg_gain&0xff));

	return gain;
}
/* ITD: Modify Dualcam By Jesse 190924 End */

static void ihdr_write_shutter_gain(struct subdrv_ctx *ctx, kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
}

static void night_mode(struct subdrv_ctx *ctx, kal_bool enable)
{
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_ov13b10lz[] = {
	// setting base on app note v2.7
	//0x0103, 0x01,
	0x0303, 0x01,
	0x0305, 0x46,
	0x0321, 0x00,
	0x0323, 0x04,
	0x0324, 0x01,
	0x0325, 0x50,
	0x0326, 0x81,
	0x0327, 0x04,
	0x3011, 0x7c,
	0x3012, 0x07,
	0x3013, 0x32,
	0x3107, 0x23,
	0x3501, 0x0c,
	0x3502, 0x10,
	0x3504, 0x08,
	0x3508, 0x07,
	0x3509, 0xc0,
	0x3600, 0x16,
	0x3601, 0x54,
	0x3612, 0x4e,
	0x3620, 0x00,
	0x3621, 0x68,
	0x3622, 0x66,
	0x3623, 0x03,
	0x3662, 0x92,
	0x3666, 0xbb,
	0x3667, 0x44,
	0x366e, 0xff,
	0x366f, 0xf3,
	0x3675, 0x44,
	0x3676, 0x00,
	0x367f, 0xe9,
	0x3681, 0x32,
	0x3682, 0x1f,
	0x3683, 0x0b,
	0x3684, 0x0b,
	0x3704, 0x0f,
	0x3706, 0x40,
	0x3708, 0x3b,
	0x3709, 0x72,
	0x370b, 0xa2,
	0x3714, 0x24,
	0x371a, 0x3e,
	0x3725, 0x42,
	0x3739, 0x12,
	0x3767, 0x00,
	0x377a, 0x0d,
	0x3789, 0x18,
	0x3790, 0x40,
	0x3791, 0xa2,
	0x37c2, 0x04,
	0x37c3, 0xf1,
	0x37d9, 0x0c,
	0x37da, 0x02,
	0x37dc, 0x02,
	0x37e1, 0x04,
	0x37e2, 0x0a,
	0x3800, 0x00,
	0x3801, 0x00,
	0x3802, 0x00,
	0x3803, 0x08,
	0x3804, 0x10,
	0x3805, 0x8f,
	0x3806, 0x0c,
	0x3807, 0x47,
	0x3808, 0x10,
	0x3809, 0x70,
	0x380a, 0x0c,
	0x380b, 0x30,
	0x380c, 0x04,
	0x380d, 0x98,
	0x380e, 0x0c,
	0x380f, 0x7c,
	0x3811, 0x0f,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3815, 0x01,
	0x3816, 0x01,
	0x3817, 0x01,
	0x381f, 0x08,
	0x3820, 0x88,
	0x3821, 0x00,
	0x3822, 0x14,
	0x3823, 0x18,
	0x3827, 0x01,
	0x382e, 0xe6,
	0x3c80, 0x00,
	0x3c87, 0x01,
	0x3c8c, 0x19,
	0x3c8d, 0x1c,
	0x3ca0, 0x00,
	0x3ca1, 0x00,
	0x3ca2, 0x00,
	0x3ca3, 0x00,
	0x3ca4, 0x50,
	0x3ca5, 0x11,
	0x3ca6, 0x01,
	0x3ca7, 0x00,
	0x3ca8, 0x00,
	0x4008, 0x02,
	0x4009, 0x0f,
	0x400a, 0x01,
	0x400b, 0x19,
	0x4011, 0x21,
	0x4017, 0x08,
	0x4019, 0x04,
	0x401a, 0x58,
	0x4032, 0x1e,
	0x4050, 0x02,
	0x4051, 0x09,
	0x405e, 0x00,
	0x4066, 0x02,
	0x4501, 0x00,
	0x4502, 0x10,
	0x4505, 0x00,
	0x4800, 0x64,
	0x481b, 0x3e,
	0x481f, 0x30,
	0x4825, 0x34,
	0x4837, 0x0e,
	0x484b, 0x01,
	0x4883, 0x02,
	0x5000, 0xff,
	0x5001, 0x0f,
	0x5045, 0x20,
	0x5046, 0x20,
	0x5047, 0xa4,
	0x5048, 0x20,
	0x5049, 0xa4,
};
#endif

static void sensor_init(struct subdrv_ctx *ctx)
{
	write_cmos_sensor(ctx,0x0103, 0x01);
	mdelay(5);
#if MULTI_WRITE
	ov13b10lz_table_write_cmos_sensor(ctx,
		addr_data_pair_init_ov13b10lz,
		sizeof(addr_data_pair_init_ov13b10lz) / sizeof(kal_uint16));
#else
	//write_cmos_sensor(ctx,0x0103, 0x01);
	write_cmos_sensor(ctx,0x0303, 0x01);
	write_cmos_sensor(ctx,0x0305, 0x46);
	write_cmos_sensor(ctx,0x0321, 0x00);
	write_cmos_sensor(ctx,0x0323, 0x04);
	write_cmos_sensor(ctx,0x0324, 0x01);
	write_cmos_sensor(ctx,0x0325, 0x50);
	write_cmos_sensor(ctx,0x0326, 0x81);
	write_cmos_sensor(ctx,0x0327, 0x04);
	write_cmos_sensor(ctx,0x3011, 0x7c);
	write_cmos_sensor(ctx,0x3012, 0x07);
	write_cmos_sensor(ctx,0x3013, 0x32);
	write_cmos_sensor(ctx,0x3107, 0x23);
	write_cmos_sensor(ctx,0x3501, 0x0c);
	write_cmos_sensor(ctx,0x3502, 0x10);
	write_cmos_sensor(ctx,0x3504, 0x08);
	write_cmos_sensor(ctx,0x3508, 0x07);
	write_cmos_sensor(ctx,0x3509, 0xc0);
	write_cmos_sensor(ctx,0x3600, 0x16);
	write_cmos_sensor(ctx,0x3601, 0x54);
	write_cmos_sensor(ctx,0x3612, 0x4e);
	write_cmos_sensor(ctx,0x3620, 0x00);
	write_cmos_sensor(ctx,0x3621, 0x68);
	write_cmos_sensor(ctx,0x3622, 0x66);
	write_cmos_sensor(ctx,0x3623, 0x03);
	write_cmos_sensor(ctx,0x3662, 0x92);
	write_cmos_sensor(ctx,0x3666, 0xbb);
	write_cmos_sensor(ctx,0x3667, 0x44);
	write_cmos_sensor(ctx,0x366e, 0xff);
	write_cmos_sensor(ctx,0x366f, 0xf3);
	write_cmos_sensor(ctx,0x3675, 0x44);
	write_cmos_sensor(ctx,0x3676, 0x00);
	write_cmos_sensor(ctx,0x367f, 0xe9);
	write_cmos_sensor(ctx,0x3681, 0x32);
	write_cmos_sensor(ctx,0x3682, 0x1f);
	write_cmos_sensor(ctx,0x3683, 0x0b);
	write_cmos_sensor(ctx,0x3684, 0x0b);
	write_cmos_sensor(ctx,0x3704, 0x0f);
	write_cmos_sensor(ctx,0x3706, 0x40);
	write_cmos_sensor(ctx,0x3708, 0x3b);
	write_cmos_sensor(ctx,0x3709, 0x72);
	write_cmos_sensor(ctx,0x370b, 0xa2);
	write_cmos_sensor(ctx,0x3714, 0x24);
	write_cmos_sensor(ctx,0x371a, 0x3e);
	write_cmos_sensor(ctx,0x3725, 0x42);
	write_cmos_sensor(ctx,0x3739, 0x12);
	write_cmos_sensor(ctx,0x3767, 0x00);
	write_cmos_sensor(ctx,0x377a, 0x0d);
	write_cmos_sensor(ctx,0x3789, 0x18);
	write_cmos_sensor(ctx,0x3790, 0x40);
	write_cmos_sensor(ctx,0x3791, 0xa2);
	write_cmos_sensor(ctx,0x37c2, 0x04);
	write_cmos_sensor(ctx,0x37c3, 0xf1);
	write_cmos_sensor(ctx,0x37d9, 0x0c);
	write_cmos_sensor(ctx,0x37da, 0x02);
	write_cmos_sensor(ctx,0x37dc, 0x02);
	write_cmos_sensor(ctx,0x37e1, 0x04);
	write_cmos_sensor(ctx,0x37e2, 0x0a);
	write_cmos_sensor(ctx,0x3800, 0x00);
	write_cmos_sensor(ctx,0x3801, 0x00);
	write_cmos_sensor(ctx,0x3802, 0x00);
	write_cmos_sensor(ctx,0x3803, 0x08);
	write_cmos_sensor(ctx,0x3804, 0x10);
	write_cmos_sensor(ctx,0x3805, 0x8f);
	write_cmos_sensor(ctx,0x3806, 0x0c);
	write_cmos_sensor(ctx,0x3807, 0x47);
	write_cmos_sensor(ctx,0x3808, 0x10);
	write_cmos_sensor(ctx,0x3809, 0x70);
	write_cmos_sensor(ctx,0x380a, 0x0c);
	write_cmos_sensor(ctx,0x380b, 0x30);
	write_cmos_sensor(ctx,0x380c, 0x04);
	write_cmos_sensor(ctx,0x380d, 0x98);
	write_cmos_sensor(ctx,0x380e, 0x0c);
	write_cmos_sensor(ctx,0x380f, 0x7c);
	write_cmos_sensor(ctx,0x3811, 0x0f);
	write_cmos_sensor(ctx,0x3813, 0x08);
	write_cmos_sensor(ctx,0x3814, 0x01);
	write_cmos_sensor(ctx,0x3815, 0x01);
	write_cmos_sensor(ctx,0x3816, 0x01);
	write_cmos_sensor(ctx,0x3817, 0x01);
	write_cmos_sensor(ctx,0x381f, 0x08);
	write_cmos_sensor(ctx,0x3820, 0x88);
	write_cmos_sensor(ctx,0x3821, 0x00);
	write_cmos_sensor(ctx,0x3822, 0x14);
	write_cmos_sensor(ctx,0x3823, 0x18);
	write_cmos_sensor(ctx,0x3827, 0x01);
	write_cmos_sensor(ctx,0x382e, 0xe6);
	write_cmos_sensor(ctx,0x3c80, 0x00);
	write_cmos_sensor(ctx,0x3c87, 0x01);
	write_cmos_sensor(ctx,0x3c8c, 0x19);
	write_cmos_sensor(ctx,0x3c8d, 0x1c);
	write_cmos_sensor(ctx,0x3ca0, 0x00);
	write_cmos_sensor(ctx,0x3ca1, 0x00);
	write_cmos_sensor(ctx,0x3ca2, 0x00);
	write_cmos_sensor(ctx,0x3ca3, 0x00);
	write_cmos_sensor(ctx,0x3ca4, 0x50);
	write_cmos_sensor(ctx,0x3ca5, 0x11);
	write_cmos_sensor(ctx,0x3ca6, 0x01);
	write_cmos_sensor(ctx,0x3ca7, 0x00);
	write_cmos_sensor(ctx,0x3ca8, 0x00);
	write_cmos_sensor(ctx,0x4008, 0x02);
	write_cmos_sensor(ctx,0x4009, 0x0f);
	write_cmos_sensor(ctx,0x400a, 0x01);
	write_cmos_sensor(ctx,0x400b, 0x19);
	write_cmos_sensor(ctx,0x4011, 0x21);
	write_cmos_sensor(ctx,0x4017, 0x08);
	write_cmos_sensor(ctx,0x4019, 0x04);
	write_cmos_sensor(ctx,0x401a, 0x58);
	write_cmos_sensor(ctx,0x4032, 0x1e);
	write_cmos_sensor(ctx,0x4050, 0x02);
	write_cmos_sensor(ctx,0x4051, 0x09);
	write_cmos_sensor(ctx,0x405e, 0x00);
	write_cmos_sensor(ctx,0x4066, 0x02);
	write_cmos_sensor(ctx,0x4501, 0x00);
	write_cmos_sensor(ctx,0x4502, 0x10);
	write_cmos_sensor(ctx,0x4505, 0x00);
	write_cmos_sensor(ctx,0x4800, 0x64);
	write_cmos_sensor(ctx,0x481b, 0x3e);
	write_cmos_sensor(ctx,0x481f, 0x30);
	write_cmos_sensor(ctx,0x4825, 0x34);
	write_cmos_sensor(ctx,0x4837, 0x0e);
	write_cmos_sensor(ctx,0x484b, 0x01);
	write_cmos_sensor(ctx,0x4883, 0x02);
	write_cmos_sensor(ctx,0x5000, 0xff);
	write_cmos_sensor(ctx,0x5001, 0x0f);
	write_cmos_sensor(ctx,0x5045, 0x20);
	write_cmos_sensor(ctx,0x5046, 0x20);
	write_cmos_sensor(ctx,0x5047, 0xa4);
	write_cmos_sensor(ctx,0x5048, 0x20);
	write_cmos_sensor(ctx,0x5049, 0xa4);
#endif
}

#if MULTI_WRITE

kal_uint16 addr_data_pair_preview_ov13b10lz[] = {
	0x0305, 0x46,
	0x0325, 0x50,
	0x0327, 0x05,
	0x3501, 0x0c,
	0x3502, 0x10,
	0x3621, 0x28,
	0x3622, 0xe6,
	0x3623, 0x00,
	0x3662, 0x92,
	0x3714, 0x24,
	0x371a, 0x3e,
	0x3739, 0x12,
	0x37c2, 0x04,
	0x37c5, 0x00,
	0x37d9, 0x0c,
	0x37e2, 0x0a,
	0x37e4, 0x04,
	0x3800, 0x00,
	0x3801, 0x00,
	0x3802, 0x00,
	0x3803, 0x08,
	0x3804, 0x10,
	0x3805, 0x8f,
	0x3806, 0x0c,
	0x3807, 0x47,
	0x3808, 0x10,
	0x3809, 0x70,
	0x380a, 0x0c,
	0x380b, 0x30,
	0x380c, 0x04,
	0x380d, 0x98,
	0x380e, 0x0c,
	0x380f, 0x7c,
	0x3811, 0x0f,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3815, 0x01,
	0x3816, 0x01,
	0x3817, 0x01,
	0x3820, 0x88,
	0x3c8c, 0x19,
	0x3f02, 0x2a,
	0x3f03, 0x10,
	0x4008, 0x02,
	0x4009, 0x0f,
	0x4050, 0x02,
	0x4051, 0x09,
	0x4500, 0x0a,
	0x4501, 0x00,
	0x4505, 0x00,
	0x4837, 0x0e,
	0x5000, 0xff,
	0x5001, 0x0f,
};

#endif

static void preview_setting(struct subdrv_ctx *ctx)
{
#if MULTI_WRITE
	ov13b10lz_table_write_cmos_sensor(ctx,
		addr_data_pair_preview_ov13b10lz,
		sizeof(addr_data_pair_preview_ov13b10lz) / sizeof(kal_uint16));
#else
	write_cmos_sensor(ctx,0x0305, 0x46);
	write_cmos_sensor(ctx,0x0325, 0x50);
	write_cmos_sensor(ctx,0x0327, 0x05);
	write_cmos_sensor(ctx,0x3501, 0x0c);
	write_cmos_sensor(ctx,0x3502, 0x10);
	write_cmos_sensor(ctx,0x3621, 0x28);
	write_cmos_sensor(ctx,0x3622, 0xe6);
	write_cmos_sensor(ctx,0x3623, 0x00);
	write_cmos_sensor(ctx,0x3662, 0x92);
	write_cmos_sensor(ctx,0x3714, 0x24);
	write_cmos_sensor(ctx,0x371a, 0x3e);
	write_cmos_sensor(ctx,0x3739, 0x12);
	write_cmos_sensor(ctx,0x37c2, 0x04);
	write_cmos_sensor(ctx,0x37c5, 0x00);
	write_cmos_sensor(ctx,0x37d9, 0x0c);
	write_cmos_sensor(ctx,0x37e2, 0x0a);
	write_cmos_sensor(ctx,0x37e4, 0x04);
	write_cmos_sensor(ctx,0x3800, 0x00);
	write_cmos_sensor(ctx,0x3801, 0x00);
	write_cmos_sensor(ctx,0x3802, 0x00);
	write_cmos_sensor(ctx,0x3803, 0x08);
	write_cmos_sensor(ctx,0x3804, 0x10);
	write_cmos_sensor(ctx,0x3805, 0x8f);
	write_cmos_sensor(ctx,0x3806, 0x0c);
	write_cmos_sensor(ctx,0x3807, 0x47);
	write_cmos_sensor(ctx,0x3808, 0x10);
	write_cmos_sensor(ctx,0x3809, 0x70);
	write_cmos_sensor(ctx,0x380a, 0x0c);
	write_cmos_sensor(ctx,0x380b, 0x30);
	write_cmos_sensor(ctx,0x380c, 0x04);
	write_cmos_sensor(ctx,0x380d, 0x98);
	write_cmos_sensor(ctx,0x380e, 0x0c);
	write_cmos_sensor(ctx,0x380f, 0x7c);
	write_cmos_sensor(ctx,0x3811, 0x0f);
	write_cmos_sensor(ctx,0x3813, 0x08);
	write_cmos_sensor(ctx,0x3814, 0x01);
	write_cmos_sensor(ctx,0x3815, 0x01);
	write_cmos_sensor(ctx,0x3816, 0x01);
	write_cmos_sensor(ctx,0x3817, 0x01);
	write_cmos_sensor(ctx,0x3820, 0x88);
	write_cmos_sensor(ctx,0x3c8c, 0x19);
	write_cmos_sensor(ctx,0x3f02, 0x2a);
	write_cmos_sensor(ctx,0x3f03, 0x10);
	write_cmos_sensor(ctx,0x4008, 0x02);
	write_cmos_sensor(ctx,0x4009, 0x0f);
	write_cmos_sensor(ctx,0x4050, 0x02);
	write_cmos_sensor(ctx,0x4051, 0x09);
	write_cmos_sensor(ctx,0x4500, 0x0a);
	write_cmos_sensor(ctx,0x4501, 0x00);
	write_cmos_sensor(ctx,0x4505, 0x00);
	write_cmos_sensor(ctx,0x4837, 0x0e);
	write_cmos_sensor(ctx,0x5000, 0xff);
	write_cmos_sensor(ctx,0x5001, 0x0f);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_15fps_ov13b10lz[] = {
	0x0305, 0x46,
	0x0325, 0x50,
	0x0327, 0x05,
	0x3501, 0x0c,
	0x3502, 0x10,
	0x3621, 0x28,
	0x3622, 0xe6,
	0x3623, 0x00,
	0x3662, 0x92,
	0x3714, 0x24,
	0x371a, 0x3e,
	0x3739, 0x12,
	0x37c2, 0x04,
	0x37c5, 0x00,
	0x37d9, 0x0c,
	0x37e2, 0x0a,
	0x37e4, 0x04,
	0x3800, 0x00,
	0x3801, 0x00,
	0x3802, 0x00,
	0x3803, 0x08,
	0x3804, 0x10,
	0x3805, 0x8f,
	0x3806, 0x0c,
	0x3807, 0x47,
	0x3808, 0x10,
	0x3809, 0x70,
	0x380a, 0x0c,
	0x380b, 0x30,
	0x380c, 0x04,
	0x380d, 0x98,
	0x380e, 0x18,
	0x380f, 0xf8,
	0x3811, 0x0f,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3815, 0x01,
	0x3816, 0x01,
	0x3817, 0x01,
	0x3820, 0x88,
	0x3c8c, 0x19,
	0x3f02, 0x2a,
	0x3f03, 0x10,
	0x4008, 0x02,
	0x4009, 0x0f,
	0x4050, 0x02,
	0x4051, 0x09,
	0x4500, 0x0a,
	0x4501, 0x00,
	0x4505, 0x00,
	0x4837, 0x0e,
	0x5000, 0xff,
	0x5001, 0x0f,
};

kal_uint16 addr_data_pair_capture_30fps_ov13b10lz[] = {
	0x0305, 0x46,
	0x0325, 0x50,
	0x0327, 0x05,
	0x3501, 0x0c,
	0x3502, 0x10,
	0x3621, 0x28,
	0x3622, 0xe6,
	0x3623, 0x00,
	0x3662, 0x92,
	0x3714, 0x24,
	0x371a, 0x3e,
	0x3739, 0x12,
	0x37c2, 0x04,
	0x37c5, 0x00,
	0x37d9, 0x0c,
	0x37e2, 0x0a,
	0x37e4, 0x04,
	0x3800, 0x00,
	0x3801, 0x00,
	0x3802, 0x00,
	0x3803, 0x08,
	0x3804, 0x10,
	0x3805, 0x8f,
	0x3806, 0x0c,
	0x3807, 0x47,
	0x3808, 0x10,
	0x3809, 0x70,
	0x380a, 0x0c,
	0x380b, 0x30,
	0x380c, 0x04,
	0x380d, 0x98,
	0x380e, 0x0c,
	0x380f, 0x7c,
	0x3811, 0x0f,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3815, 0x01,
	0x3816, 0x01,
	0x3817, 0x01,
	0x3820, 0x88,
	0x3c8c, 0x19,
	0x3f02, 0x2a,
	0x3f03, 0x10,
	0x4008, 0x02,
	0x4009, 0x0f,
	0x4050, 0x02,
	0x4051, 0x09,
	0x4500, 0x0a,
	0x4501, 0x00,
	0x4505, 0x00,
	0x4837, 0x0e,
	0x5000, 0xff,
	0x5001, 0x0f,
};
#endif

static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{

#if MULTI_WRITE
	if (currefps == 150) {
		ov13b10lz_table_write_cmos_sensor(ctx,
			addr_data_pair_capture_15fps_ov13b10lz,
			sizeof(addr_data_pair_capture_15fps_ov13b10lz) /
			sizeof(kal_uint16));
	} else {
		ov13b10lz_table_write_cmos_sensor(ctx,
			addr_data_pair_capture_30fps_ov13b10lz,
			sizeof(addr_data_pair_capture_30fps_ov13b10lz) /
			sizeof(kal_uint16));
	}
#else
	if (currefps == 150) {
	write_cmos_sensor(ctx,0x0305, 0x46);
	write_cmos_sensor(ctx,0x0325, 0x50);
	write_cmos_sensor(ctx,0x0327, 0x05);
	write_cmos_sensor(ctx,0x3501, 0x0c);
	write_cmos_sensor(ctx,0x3502, 0x10);
	write_cmos_sensor(ctx,0x3621, 0x28);
	write_cmos_sensor(ctx,0x3622, 0xe6);
	write_cmos_sensor(ctx,0x3623, 0x00);
	write_cmos_sensor(ctx,0x3662, 0x92);
	write_cmos_sensor(ctx,0x3714, 0x24);
	write_cmos_sensor(ctx,0x371a, 0x3e);
	write_cmos_sensor(ctx,0x3739, 0x12);
	write_cmos_sensor(ctx,0x37c2, 0x04);
	write_cmos_sensor(ctx,0x37c5, 0x00);
	write_cmos_sensor(ctx,0x37d9, 0x0c);
	write_cmos_sensor(ctx,0x37e2, 0x0a);
	write_cmos_sensor(ctx,0x37e4, 0x04);
	write_cmos_sensor(ctx,0x3800, 0x00);
	write_cmos_sensor(ctx,0x3801, 0x00);
	write_cmos_sensor(ctx,0x3802, 0x00);
	write_cmos_sensor(ctx,0x3803, 0x08);
	write_cmos_sensor(ctx,0x3804, 0x10);
	write_cmos_sensor(ctx,0x3805, 0x8f);
	write_cmos_sensor(ctx,0x3806, 0x0c);
	write_cmos_sensor(ctx,0x3807, 0x47);
	write_cmos_sensor(ctx,0x3808, 0x10);
	write_cmos_sensor(ctx,0x3809, 0x70);
	write_cmos_sensor(ctx,0x380a, 0x0c);
	write_cmos_sensor(ctx,0x380b, 0x30);
	write_cmos_sensor(ctx,0x380c, 0x04);
	write_cmos_sensor(ctx,0x380d, 0x98);
	write_cmos_sensor(ctx,0x380e, 0x18);
	write_cmos_sensor(ctx,0x380f, 0xf8);
	write_cmos_sensor(ctx,0x3811, 0x0f);
	write_cmos_sensor(ctx,0x3813, 0x08);
	write_cmos_sensor(ctx,0x3814, 0x01);
	write_cmos_sensor(ctx,0x3815, 0x01);
	write_cmos_sensor(ctx,0x3816, 0x01);
	write_cmos_sensor(ctx,0x3817, 0x01);
	write_cmos_sensor(ctx,0x3820, 0x88);
	write_cmos_sensor(ctx,0x3c8c, 0x19);
	write_cmos_sensor(ctx,0x3f02, 0x2a);
	write_cmos_sensor(ctx,0x3f03, 0x10);
	write_cmos_sensor(ctx,0x4008, 0x02);
	write_cmos_sensor(ctx,0x4009, 0x0f);
	write_cmos_sensor(ctx,0x4050, 0x02);
	write_cmos_sensor(ctx,0x4051, 0x09);
	write_cmos_sensor(ctx,0x4500, 0x0a);
	write_cmos_sensor(ctx,0x4501, 0x00);
	write_cmos_sensor(ctx,0x4505, 0x00);
	write_cmos_sensor(ctx,0x4837, 0x0e);
	write_cmos_sensor(ctx,0x5000, 0xff);
	write_cmos_sensor(ctx,0x5001, 0x0f);
	} else {
	write_cmos_sensor(ctx,0x0305, 0x46);
	write_cmos_sensor(ctx,0x0325, 0x50);
	write_cmos_sensor(ctx,0x0327, 0x05);
	write_cmos_sensor(ctx,0x3501, 0x0c);
	write_cmos_sensor(ctx,0x3502, 0x10);
	write_cmos_sensor(ctx,0x3621, 0x28);
	write_cmos_sensor(ctx,0x3622, 0xe6);
	write_cmos_sensor(ctx,0x3623, 0x00);
	write_cmos_sensor(ctx,0x3662, 0x92);
	write_cmos_sensor(ctx,0x3714, 0x24);
	write_cmos_sensor(ctx,0x371a, 0x3e);
	write_cmos_sensor(ctx,0x3739, 0x12);
	write_cmos_sensor(ctx,0x37c2, 0x04);
	write_cmos_sensor(ctx,0x37c5, 0x00);
	write_cmos_sensor(ctx,0x37d9, 0x0c);
	write_cmos_sensor(ctx,0x37e2, 0x0a);
	write_cmos_sensor(ctx,0x37e4, 0x04);
	write_cmos_sensor(ctx,0x3800, 0x00);
	write_cmos_sensor(ctx,0x3801, 0x00);
	write_cmos_sensor(ctx,0x3802, 0x00);
	write_cmos_sensor(ctx,0x3803, 0x08);
	write_cmos_sensor(ctx,0x3804, 0x10);
	write_cmos_sensor(ctx,0x3805, 0x8f);
	write_cmos_sensor(ctx,0x3806, 0x0c);
	write_cmos_sensor(ctx,0x3807, 0x47);
	write_cmos_sensor(ctx,0x3808, 0x10);
	write_cmos_sensor(ctx,0x3809, 0x70);
	write_cmos_sensor(ctx,0x380a, 0x0c);
	write_cmos_sensor(ctx,0x380b, 0x30);
	write_cmos_sensor(ctx,0x380c, 0x04);
	write_cmos_sensor(ctx,0x380d, 0x98);
	write_cmos_sensor(ctx,0x380e, 0x0c);
	write_cmos_sensor(ctx,0x380f, 0x7c);
	write_cmos_sensor(ctx,0x3811, 0x0f);
	write_cmos_sensor(ctx,0x3813, 0x08);
	write_cmos_sensor(ctx,0x3814, 0x01);
	write_cmos_sensor(ctx,0x3815, 0x01);
	write_cmos_sensor(ctx,0x3816, 0x01);
	write_cmos_sensor(ctx,0x3817, 0x01);
	write_cmos_sensor(ctx,0x3820, 0x88);
	write_cmos_sensor(ctx,0x3c8c, 0x19);
	write_cmos_sensor(ctx,0x3f02, 0x2a);
	write_cmos_sensor(ctx,0x3f03, 0x10);
	write_cmos_sensor(ctx,0x4008, 0x02);
	write_cmos_sensor(ctx,0x4009, 0x0f);
	write_cmos_sensor(ctx,0x4050, 0x02);
	write_cmos_sensor(ctx,0x4051, 0x09);
	write_cmos_sensor(ctx,0x4500, 0x0a);
	write_cmos_sensor(ctx,0x4501, 0x00);
	write_cmos_sensor(ctx,0x4505, 0x00);
	write_cmos_sensor(ctx,0x4837, 0x0e);
	write_cmos_sensor(ctx,0x5000, 0xff);
	write_cmos_sensor(ctx,0x5001, 0x0f);
	}
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_video_ov13b10lz[] = {
	0x0305, 0x46,
	0x0325, 0x50,
	0x0327, 0x05,
	0x3501, 0x0c,
	0x3502, 0x10,
	0x3621, 0x28,
	0x3622, 0xe6,
	0x3623, 0x00,
	0x3662, 0x92,
	0x3714, 0x24,
	0x371a, 0x3e,
	0x3739, 0x12,
	0x37c2, 0x04,
	0x37c5, 0x00,
	0x37d9, 0x0c,
	0x37e2, 0x0a,
	0x37e4, 0x04,
	0x3800, 0x00,
	0x3801, 0x00,
	0x3802, 0x00,
	0x3803, 0x08,
	0x3804, 0x10,
	0x3805, 0x8f,
	0x3806, 0x0c,
	0x3807, 0x47,
	0x3808, 0x10,
	0x3809, 0x70,
	0x380a, 0x0c,
	0x380b, 0x30,
	0x380c, 0x04,
	0x380d, 0x98,
	0x380e, 0x0c,
	0x380f, 0x7c,
	0x3811, 0x0f,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3815, 0x01,
	0x3816, 0x01,
	0x3817, 0x01,
	0x3820, 0x88,
	0x3c8c, 0x19,
	0x3f02, 0x2a,
	0x3f03, 0x10,
	0x4008, 0x02,
	0x4009, 0x0f,
	0x4050, 0x02,
	0x4051, 0x09,
	0x4500, 0x0a,
	0x4501, 0x00,
	0x4505, 0x00,
	0x4837, 0x0e,
	0x5000, 0xff,
	0x5001, 0x0f,
};
#endif

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
#if MULTI_WRITE
	ov13b10lz_table_write_cmos_sensor(ctx,
		addr_data_pair_video_ov13b10lz,
		sizeof(addr_data_pair_video_ov13b10lz) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(ctx,0x0305, 0x46);
	write_cmos_sensor(ctx,0x0325, 0x50);
	write_cmos_sensor(ctx,0x0327, 0x05);
	write_cmos_sensor(ctx,0x3501, 0x0c);
	write_cmos_sensor(ctx,0x3502, 0x10);
	write_cmos_sensor(ctx,0x3621, 0x28);
	write_cmos_sensor(ctx,0x3622, 0xe6);
	write_cmos_sensor(ctx,0x3623, 0x00);
	write_cmos_sensor(ctx,0x3662, 0x92);
	write_cmos_sensor(ctx,0x3714, 0x24);
	write_cmos_sensor(ctx,0x371a, 0x3e);
	write_cmos_sensor(ctx,0x3739, 0x12);
	write_cmos_sensor(ctx,0x37c2, 0x04);
	write_cmos_sensor(ctx,0x37c5, 0x00);
	write_cmos_sensor(ctx,0x37d9, 0x0c);
	write_cmos_sensor(ctx,0x37e2, 0x0a);
	write_cmos_sensor(ctx,0x37e4, 0x04);
	write_cmos_sensor(ctx,0x3800, 0x00);
	write_cmos_sensor(ctx,0x3801, 0x00);
	write_cmos_sensor(ctx,0x3802, 0x00);
	write_cmos_sensor(ctx,0x3803, 0x08);
	write_cmos_sensor(ctx,0x3804, 0x10);
	write_cmos_sensor(ctx,0x3805, 0x8f);
	write_cmos_sensor(ctx,0x3806, 0x0c);
	write_cmos_sensor(ctx,0x3807, 0x47);
	write_cmos_sensor(ctx,0x3808, 0x10);
	write_cmos_sensor(ctx,0x3809, 0x70);
	write_cmos_sensor(ctx,0x380a, 0x0c);
	write_cmos_sensor(ctx,0x380b, 0x30);
	write_cmos_sensor(ctx,0x380c, 0x04);
	write_cmos_sensor(ctx,0x380d, 0x98);
	write_cmos_sensor(ctx,0x380e, 0x0c);
	write_cmos_sensor(ctx,0x380f, 0x7c);
	write_cmos_sensor(ctx,0x3811, 0x0f);
	write_cmos_sensor(ctx,0x3813, 0x08);
	write_cmos_sensor(ctx,0x3814, 0x01);
	write_cmos_sensor(ctx,0x3815, 0x01);
	write_cmos_sensor(ctx,0x3816, 0x01);
	write_cmos_sensor(ctx,0x3817, 0x01);
	write_cmos_sensor(ctx,0x3820, 0x88);
	write_cmos_sensor(ctx,0x3c8c, 0x19);
	write_cmos_sensor(ctx,0x3f02, 0x2a);
	write_cmos_sensor(ctx,0x3f03, 0x10);
	write_cmos_sensor(ctx,0x4008, 0x02);
	write_cmos_sensor(ctx,0x4009, 0x0f);
	write_cmos_sensor(ctx,0x4050, 0x02);
	write_cmos_sensor(ctx,0x4051, 0x09);
	write_cmos_sensor(ctx,0x4500, 0x0a);
	write_cmos_sensor(ctx,0x4501, 0x00);
	write_cmos_sensor(ctx,0x4505, 0x00);
	write_cmos_sensor(ctx,0x4837, 0x0e);
	write_cmos_sensor(ctx,0x5000, 0xff);
	write_cmos_sensor(ctx,0x5001, 0x0f);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_ov13b10lz[] = {
	0x0305, 0x23,
	0x0325, 0x50,
	0x0327, 0x04,
	0x3501, 0x03,
	0x3502, 0x00,
	0x3621, 0x68,
	0x3622, 0x66,
	0x3623, 0x03,
	0x3662, 0x88,
	0x3714, 0x28,
	0x371a, 0x3e,
	0x3739, 0x10,
	0x37c2, 0x14,
	0x37c5, 0x00,
	0x37d9, 0x06,
	0x37e2, 0x0c,
	0x37e4, 0x00,
	0x3800, 0x03,
	0x3801, 0x30,
	0x3802, 0x03,
	0x3803, 0x48,
	0x3804, 0x0d,
	0x3805, 0x5f,
	0x3806, 0x09,
	0x3807, 0x07,
	0x3808, 0x05,
	0x3809, 0x00,
	0x380a, 0x02,
	0x380b, 0xd0,
	0x380c, 0x04,
	0x380d, 0x98,
	0x380e, 0x03,
	0x380f, 0x1e,
	0x3811, 0x0b,
	0x3813, 0x08,
	0x3814, 0x03,
	0x3815, 0x01,
	0x3816, 0x03,
	0x3817, 0x01,
	0x3820, 0x8b,
	0x3c8c, 0x18,
	0x3f02, 0x0f,
	0x3f03, 0x00,
	0x4008, 0x00,
	0x4009, 0x05,
	0x4050, 0x00,
	0x4051, 0x05,
	0x4500, 0x0a,
	0x4501, 0x08,
	0x4505, 0x04,
	0x4837, 0x1d,
	0x5000, 0xfd,
	0x5001, 0x0d,
};
#endif

static void hs_video_setting(struct subdrv_ctx *ctx)
{
#if MULTI_WRITE
	ov13b10lz_table_write_cmos_sensor(ctx,
		addr_data_pair_hs_video_ov13b10lz,
		sizeof(addr_data_pair_hs_video_ov13b10lz) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(ctx,0x0305, 0x23);
	write_cmos_sensor(ctx,0x0325, 0x50);
	write_cmos_sensor(ctx,0x0327, 0x04);
	write_cmos_sensor(ctx,0x3501, 0x03);
	write_cmos_sensor(ctx,0x3502, 0x00);
	write_cmos_sensor(ctx,0x3621, 0x68);
	write_cmos_sensor(ctx,0x3622, 0x66);
	write_cmos_sensor(ctx,0x3623, 0x03);
	write_cmos_sensor(ctx,0x3662, 0x88);
	write_cmos_sensor(ctx,0x3714, 0x28);
	write_cmos_sensor(ctx,0x371a, 0x3e);
	write_cmos_sensor(ctx,0x3739, 0x10);
	write_cmos_sensor(ctx,0x37c2, 0x14);
	write_cmos_sensor(ctx,0x37c5, 0x00);
	write_cmos_sensor(ctx,0x37d9, 0x06);
	write_cmos_sensor(ctx,0x37e2, 0x0c);
	write_cmos_sensor(ctx,0x37e4, 0x00);
	write_cmos_sensor(ctx,0x3800, 0x03);
	write_cmos_sensor(ctx,0x3801, 0x30);
	write_cmos_sensor(ctx,0x3802, 0x03);
	write_cmos_sensor(ctx,0x3803, 0x48);
	write_cmos_sensor(ctx,0x3804, 0x0d);
	write_cmos_sensor(ctx,0x3805, 0x5f);
	write_cmos_sensor(ctx,0x3806, 0x09);
	write_cmos_sensor(ctx,0x3807, 0x07);
	write_cmos_sensor(ctx,0x3808, 0x05);
	write_cmos_sensor(ctx,0x3809, 0x00);
	write_cmos_sensor(ctx,0x380a, 0x02);
	write_cmos_sensor(ctx,0x380b, 0xd0);
	write_cmos_sensor(ctx,0x380c, 0x04);
	write_cmos_sensor(ctx,0x380d, 0x98);
	write_cmos_sensor(ctx,0x380e, 0x03);
	write_cmos_sensor(ctx,0x380f, 0x1e);
	write_cmos_sensor(ctx,0x3811, 0x0b);
	write_cmos_sensor(ctx,0x3813, 0x08);
	write_cmos_sensor(ctx,0x3814, 0x03);
	write_cmos_sensor(ctx,0x3815, 0x01);
	write_cmos_sensor(ctx,0x3816, 0x03);
	write_cmos_sensor(ctx,0x3817, 0x01);
	write_cmos_sensor(ctx,0x3820, 0x8b);
	write_cmos_sensor(ctx,0x3c8c, 0x18);
	write_cmos_sensor(ctx,0x3f02, 0x0f);
	write_cmos_sensor(ctx,0x3f03, 0x00);
	write_cmos_sensor(ctx,0x4008, 0x00);
	write_cmos_sensor(ctx,0x4009, 0x05);
	write_cmos_sensor(ctx,0x4050, 0x00);
	write_cmos_sensor(ctx,0x4051, 0x05);
	write_cmos_sensor(ctx,0x4500, 0x0a);
	write_cmos_sensor(ctx,0x4501, 0x08);
	write_cmos_sensor(ctx,0x4505, 0x04);
	write_cmos_sensor(ctx,0x4837, 0x1d);
	write_cmos_sensor(ctx,0x5000, 0xfd);
	write_cmos_sensor(ctx,0x5001, 0x0d);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_ov13b10lz[] = {
	0x0305, 0x23,
	0x0325, 0x50,
	0x0327, 0x04,
	0x3501, 0x06,
	0x3502, 0x00,
	0x3621, 0x68,
	0x3622, 0x66,
	0x3623, 0x03,
	0x3662, 0x88,
	0x3714, 0x28,
	0x371a, 0x3e,
	0x3739, 0x10,
	0x37c2, 0x14,
	0x37c5, 0x00,
	0x37d9, 0x06,
	0x37e2, 0x0c,
	0x37e4, 0x00,
	0x3800, 0x00,
	0x3801, 0xb0,
	0x3802, 0x01,
	0x3803, 0xe0,
	0x3804, 0x0f,
	0x3805, 0xdf,
	0x3806, 0x0a,
	0x3807, 0x6f,
	0x3808, 0x07,
	0x3809, 0x80,
	0x380a, 0x04,
	0x380b, 0x38,
	0x380c, 0x04,
	0x380d, 0x98,
	0x380e, 0x06,
	0x380f, 0x34,
	0x3811, 0x0b,
	0x3813, 0x08,
	0x3814, 0x03,
	0x3815, 0x01,
	0x3816, 0x03,
	0x3817, 0x01,
	0x3820, 0x8b,
	0x3c8c, 0x18,
	0x3f02, 0x0f,
	0x3f03, 0x00,
	0x4008, 0x00,
	0x4009, 0x05,
	0x4050, 0x00,
	0x4051, 0x05,
	0x4500, 0x0a,
	0x4501, 0x08,
	0x4505, 0x04,
	0x4837, 0x1d,
	0x5000, 0xfd,
	0x5001, 0x0d,
};
#endif

static void slim_video_setting(struct subdrv_ctx *ctx)
{
#if MULTI_WRITE
	ov13b10lz_table_write_cmos_sensor(ctx,
		addr_data_pair_slim_video_ov13b10lz,
		sizeof(addr_data_pair_slim_video_ov13b10lz) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(ctx,0x0305, 0x23);
	write_cmos_sensor(ctx,0x0325, 0x50);
	write_cmos_sensor(ctx,0x0327, 0x04);
	write_cmos_sensor(ctx,0x3501, 0x06);
	write_cmos_sensor(ctx,0x3502, 0x00);
	write_cmos_sensor(ctx,0x3621, 0x68);
	write_cmos_sensor(ctx,0x3622, 0x66);
	write_cmos_sensor(ctx,0x3623, 0x03);
	write_cmos_sensor(ctx,0x3662, 0x88);
	write_cmos_sensor(ctx,0x3714, 0x28);
	write_cmos_sensor(ctx,0x371a, 0x3e);
	write_cmos_sensor(ctx,0x3739, 0x10);
	write_cmos_sensor(ctx,0x37c2, 0x14);
	write_cmos_sensor(ctx,0x37c5, 0x00);
	write_cmos_sensor(ctx,0x37d9, 0x06);
	write_cmos_sensor(ctx,0x37e2, 0x0c);
	write_cmos_sensor(ctx,0x37e4, 0x00);
	write_cmos_sensor(ctx,0x3800, 0x00);
	write_cmos_sensor(ctx,0x3801, 0xb0);
	write_cmos_sensor(ctx,0x3802, 0x01);
	write_cmos_sensor(ctx,0x3803, 0xe0);
	write_cmos_sensor(ctx,0x3804, 0x0f);
	write_cmos_sensor(ctx,0x3805, 0xdf);
	write_cmos_sensor(ctx,0x3806, 0x0a);
	write_cmos_sensor(ctx,0x3807, 0x6f);
	write_cmos_sensor(ctx,0x3808, 0x07);
	write_cmos_sensor(ctx,0x3809, 0x80);
	write_cmos_sensor(ctx,0x380a, 0x04);
	write_cmos_sensor(ctx,0x380b, 0x38);
	write_cmos_sensor(ctx,0x380c, 0x04);
	write_cmos_sensor(ctx,0x380d, 0x98);
	write_cmos_sensor(ctx,0x380e, 0x06);
	write_cmos_sensor(ctx,0x380f, 0x34);
	write_cmos_sensor(ctx,0x3811, 0x0b);
	write_cmos_sensor(ctx,0x3813, 0x08);
	write_cmos_sensor(ctx,0x3814, 0x03);
	write_cmos_sensor(ctx,0x3815, 0x01);
	write_cmos_sensor(ctx,0x3816, 0x03);
	write_cmos_sensor(ctx,0x3817, 0x01);
	write_cmos_sensor(ctx,0x3820, 0x8b);
	write_cmos_sensor(ctx,0x3c8c, 0x18);
	write_cmos_sensor(ctx,0x3f02, 0x0f);
	write_cmos_sensor(ctx,0x3f03, 0x00);
	write_cmos_sensor(ctx,0x4008, 0x00);
	write_cmos_sensor(ctx,0x4009, 0x05);
	write_cmos_sensor(ctx,0x4050, 0x00);
	write_cmos_sensor(ctx,0x4051, 0x05);
	write_cmos_sensor(ctx,0x4500, 0x0a);
	write_cmos_sensor(ctx,0x4501, 0x08);
	write_cmos_sensor(ctx,0x4505, 0x04);
	write_cmos_sensor(ctx,0x4837, 0x1d);
	write_cmos_sensor(ctx,0x5000, 0xfd);
	write_cmos_sensor(ctx,0x5001, 0x0d);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_custom1_ov13b10lz[] = {
	0x0305, 0x46,
	0x0325, 0x50,
	0x0327, 0x05,
	0x3501, 0x0c,
	0x3502, 0x10,
	0x3621, 0x28,
	0x3622, 0xe6,
	0x3623, 0x00,
	0x3662, 0x92,
	0x3714, 0x24,
	0x371a, 0x3e,
	0x3739, 0x12,
	0x37c2, 0x04,
	0x37c5, 0x00,
	0x37d9, 0x0c,
	0x37e2, 0x0a,
	0x37e4, 0x04,
	0x3800, 0x00,
	0x3801, 0x00,
	0x3802, 0x00,
	0x3803, 0x08,
	0x3804, 0x10,
	0x3805, 0x8f,
	0x3806, 0x0c,
	0x3807, 0x47,
	0x3808, 0x10,
	0x3809, 0x70,
	0x380a, 0x0c,
	0x380b, 0x30,
	0x380c, 0x04,
	0x380d, 0x98,
	0x380e, 0x0c,
	0x380f, 0x7c,
	0x3811, 0x0f,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3815, 0x01,
	0x3816, 0x01,
	0x3817, 0x01,
	0x3820, 0x88,
	0x3c8c, 0x19,
	0x3f02, 0x2a,
	0x3f03, 0x10,
	0x4008, 0x02,
	0x4009, 0x0f,
	0x4050, 0x02,
	0x4051, 0x09,
	0x4500, 0x0a,
	0x4501, 0x00,
	0x4505, 0x00,
	0x4837, 0x0e,
	0x5000, 0xff,
	0x5001, 0x0f,
};
#endif

static void custom1_setting(struct subdrv_ctx *ctx)
{
#if MULTI_WRITE
	ov13b10lz_table_write_cmos_sensor(ctx,
		addr_data_pair_custom1_ov13b10lz,
		sizeof(addr_data_pair_custom1_ov13b10lz) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(ctx,0x0305, 0x46);
	write_cmos_sensor(ctx,0x0325, 0x50);
	write_cmos_sensor(ctx,0x0327, 0x05);
	write_cmos_sensor(ctx,0x3501, 0x0c);
	write_cmos_sensor(ctx,0x3502, 0x10);
	write_cmos_sensor(ctx,0x3621, 0x28);
	write_cmos_sensor(ctx,0x3622, 0xe6);
	write_cmos_sensor(ctx,0x3623, 0x00);
	write_cmos_sensor(ctx,0x3662, 0x92);
	write_cmos_sensor(ctx,0x3714, 0x24);
	write_cmos_sensor(ctx,0x371a, 0x3e);
	write_cmos_sensor(ctx,0x3739, 0x12);
	write_cmos_sensor(ctx,0x37c2, 0x04);
	write_cmos_sensor(ctx,0x37c5, 0x00);
	write_cmos_sensor(ctx,0x37d9, 0x0c);
	write_cmos_sensor(ctx,0x37e2, 0x0a);
	write_cmos_sensor(ctx,0x37e4, 0x04);
	write_cmos_sensor(ctx,0x3800, 0x00);
	write_cmos_sensor(ctx,0x3801, 0x00);
	write_cmos_sensor(ctx,0x3802, 0x00);
	write_cmos_sensor(ctx,0x3803, 0x08);
	write_cmos_sensor(ctx,0x3804, 0x10);
	write_cmos_sensor(ctx,0x3805, 0x8f);
	write_cmos_sensor(ctx,0x3806, 0x0c);
	write_cmos_sensor(ctx,0x3807, 0x47);
	write_cmos_sensor(ctx,0x3808, 0x10);
	write_cmos_sensor(ctx,0x3809, 0x70);
	write_cmos_sensor(ctx,0x380a, 0x0c);
	write_cmos_sensor(ctx,0x380b, 0x30);
	write_cmos_sensor(ctx,0x380c, 0x04);
	write_cmos_sensor(ctx,0x380d, 0x98);
	write_cmos_sensor(ctx,0x380e, 0x0c);
	write_cmos_sensor(ctx,0x380f, 0x7c);
	write_cmos_sensor(ctx,0x3811, 0x0f);
	write_cmos_sensor(ctx,0x3813, 0x08);
	write_cmos_sensor(ctx,0x3814, 0x01);
	write_cmos_sensor(ctx,0x3815, 0x01);
	write_cmos_sensor(ctx,0x3816, 0x01);
	write_cmos_sensor(ctx,0x3817, 0x01);
	write_cmos_sensor(ctx,0x3820, 0x88);
	write_cmos_sensor(ctx,0x3c8c, 0x19);
	write_cmos_sensor(ctx,0x3f02, 0x2a);
	write_cmos_sensor(ctx,0x3f03, 0x10);
	write_cmos_sensor(ctx,0x4008, 0x02);
	write_cmos_sensor(ctx,0x4009, 0x0f);
	write_cmos_sensor(ctx,0x4050, 0x02);
	write_cmos_sensor(ctx,0x4051, 0x09);
	write_cmos_sensor(ctx,0x4500, 0x0a);
	write_cmos_sensor(ctx,0x4501, 0x00);
	write_cmos_sensor(ctx,0x4505, 0x00);
	write_cmos_sensor(ctx,0x4837, 0x0e);
	write_cmos_sensor(ctx,0x5000, 0xff);
	write_cmos_sensor(ctx,0x5001, 0x0f);
#endif
}

static kal_uint32 return_sensor_id(struct subdrv_ctx *ctx)
{
   
	return (
		(read_cmos_sensor(ctx,0x300b) << 8) | read_cmos_sensor(ctx,0x300c));
}

static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
	do {
		*sensor_id = return_sensor_id(ctx);
	if (*sensor_id == imgsensor_info.sensor_id) {
		cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
			ctx->i2c_write_id, *sensor_id);
		return ERROR_NONE;
	}
		retry--;
	} while (retry > 0);
	i++;
	retry = 1;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		cam_pr_debug("ov13b10lz imgsensor id: 0x%x fail\n", *sensor_id);
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}

static int open(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 1;
	kal_uint32 sensor_id = 0;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];

		do {
			sensor_id = return_sensor_id(ctx);
			if (sensor_id == imgsensor_info.sensor_id) {
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
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
		cam_pr_debug("Open sensor id: 0x%x fail\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}

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
	//ctx->ihdr_en = 0;
	ctx->test_pattern = 0;
	ctx->current_fps = imgsensor_info.pre.max_framerate;
	ctx->pdaf_mode = 0;


	return ERROR_NONE;
}

static int close(struct subdrv_ctx *ctx)
{
	return ERROR_NONE;
}   /*  close  */

static kal_uint32 preview(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	preview_setting(ctx);
	return ERROR_NONE;
}

static kal_uint32 capture(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (ctx->current_fps == imgsensor_info.cap1.max_framerate) {
		ctx->pclk = imgsensor_info.cap1.pclk;
		ctx->line_length = imgsensor_info.cap1.linelength;
		ctx->frame_length = imgsensor_info.cap1.framelength;
		ctx->min_frame_length = imgsensor_info.cap1.framelength;
	} else {
		if (ctx->current_fps != imgsensor_info.cap.max_framerate)
			cam_pr_debug("current_fps %d fps is not support,use cap1: %d fps!\n",
				ctx->current_fps,
				imgsensor_info.cap1.max_framerate/10);
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	}
	capture_setting(ctx, ctx->current_fps);
	return ERROR_NONE;
} /* capture(ctx) */

static kal_uint32 normal_video(struct subdrv_ctx *ctx,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("%s E\n", __func__);
	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	normal_video_setting(ctx, ctx->current_fps);
	return ERROR_NONE;
}

static kal_uint32 hs_video(struct subdrv_ctx *ctx,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("%s E\n", __func__);
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
}

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("%s E\n", __func__);
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
}

/* ITD: Modify Dualcam By Jesse 190924 Start */
static kal_uint32 custom1(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[ov13b10lz] custom1 mode start\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom1_setting(ctx);
	return ERROR_NONE;
}   /* custom1   */



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
}   /*  get_resolution  */

static int get_info(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
		      MSDK_SENSOR_INFO_STRUCT *sensor_info,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	//sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
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
	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode*/
	sensor_info->PDAF_Support = 0;  //fuzr change from 1 t 0

	//sensor_info->HDR_Support = 0; /*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

//fuzr	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
//fuzr	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
	sensor_info->SensorPacketECCOrder = 1;
/*  no use confirm 
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = ctx->info.pre.startx;
		sensor_info->SensorGrabStartY = ctx->info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			ctx->info.pre.mipi_data_lp2hs_settle_dc;

	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = ctx->info.cap.startx;
		sensor_info->SensorGrabStartY = ctx->info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			ctx->info.cap.mipi_data_lp2hs_settle_dc;

	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_info->SensorGrabStartX =
			ctx->info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			ctx->info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			ctx->info.normal_video.mipi_data_lp2hs_settle_dc;

	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = ctx->info.hs_video.startx;
		sensor_info->SensorGrabStartY = ctx->info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			ctx->info.hs_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX =
			ctx->info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			ctx->info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			ctx->info.slim_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = ctx->info.custom1.startx;
		sensor_info->SensorGrabStartY = ctx->info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			ctx->info.custom1.mipi_data_lp2hs_settle_dc;
	break;

	default:
		sensor_info->SensorGrabStartX = ctx->info.pre.startx;
		sensor_info->SensorGrabStartY = ctx->info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			ctx->info.pre.mipi_data_lp2hs_settle_dc;
	break;
	}
*/
	return ERROR_NONE;
}   /*  get_info  */


static int control(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);
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


	default:
		cam_pr_debug("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
	return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}   /* control(ctx) */

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
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

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx, kal_bool enable,
			UINT16 framerate)
{
	cam_pr_debug("enable = %d, framerate = %d\n",
		enable, framerate);

	if (enable) //enable auto flicker
		ctx->autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		ctx->autoflicker_en = KAL_FALSE;

	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frameHeight;

	cam_pr_debug("scenario_id = %d, framerate = %d\n",
			scenario_id, framerate);

	if (framerate == 0)
		return ERROR_NONE;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	    frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frameHeight > imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength):0;
	    ctx->frame_length = imgsensor_info.pre.framelength +
			ctx->dummy_line;
	    ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
	break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	    frameHeight = imgsensor_info.normal_video.pclk / framerate * 10 /
				imgsensor_info.normal_video.linelength;
		ctx->dummy_line = (frameHeight >
			imgsensor_info.normal_video.framelength) ?
		(frameHeight - imgsensor_info.normal_video.framelength):0;
	    ctx->frame_length = imgsensor_info.normal_video.framelength +
			ctx->dummy_line;
	    ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
	break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	    frameHeight = imgsensor_info.cap.pclk / framerate * 10 /
			imgsensor_info.cap.linelength;

		ctx->dummy_line =
			(frameHeight > imgsensor_info.cap.framelength) ?
			(frameHeight - imgsensor_info.cap.framelength):0;
	    ctx->frame_length = imgsensor_info.cap.framelength +
			ctx->dummy_line;
	    ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
	break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	    frameHeight = imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
			(frameHeight > imgsensor_info.hs_video.framelength) ?
			(frameHeight - imgsensor_info.hs_video.framelength):0;
		ctx->frame_length = imgsensor_info.hs_video.framelength +
			ctx->dummy_line;
	    ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
	break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	    frameHeight = imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		ctx->dummy_line = (frameHeight >
			imgsensor_info.slim_video.framelength) ?
			(frameHeight - imgsensor_info.slim_video.framelength):0;
	    ctx->frame_length = imgsensor_info.slim_video.framelength +
			ctx->dummy_line;
	    ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
	break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
	    frameHeight = imgsensor_info.custom1.pclk / framerate * 10 /
			imgsensor_info.custom1.linelength;
		ctx->dummy_line = (frameHeight >
			imgsensor_info.custom1.framelength) ?
			(frameHeight - imgsensor_info.custom1.framelength):0;
	    ctx->frame_length = imgsensor_info.custom1.framelength +
			ctx->dummy_line;
	    ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
	break;

	default:  //coding with  preview scenario by default
	    frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		ctx->dummy_line = (frameHeight >
			imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength):0;
	    ctx->frame_length = imgsensor_info.pre.framelength +
			ctx->dummy_line;
	    ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
	break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 *framerate)
{
	cam_pr_debug("[3058]scenario_id = %d\n", scenario_id);

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
	
	default:
	break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_uint32 modes)
{
	if (modes != ctx->test_pattern)
		pr_debug("Test_Pattern modes: %d -> %d\n", ctx->test_pattern, modes);
	memset(_i2c_data, 0x0, sizeof(_i2c_data));
	_size_to_write = 0;
	if (modes == 2) {
		_i2c_data[_size_to_write++] = 0x5000;
		_i2c_data[_size_to_write++] = 0x81;//10000001
		_i2c_data[_size_to_write++] = 0x5001;
		_i2c_data[_size_to_write++] = 0x00;
		_i2c_data[_size_to_write++] = 0x5002;
		_i2c_data[_size_to_write++] = 0x92;//10010010
		/* need check with vendor */
		_i2c_data[_size_to_write++] = 0x5081;
		_i2c_data[_size_to_write++] = 0x01;
	} else if (modes == 5) { //black
		//@@ Solid color BLACK - on
		//6c 3019 f0; d2
		//6c 4308 01 ;
		_i2c_data[_size_to_write++] = 0x3019;
		_i2c_data[_size_to_write++] = 0xf0;
		_i2c_data[_size_to_write++] = 0x4308;
		_i2c_data[_size_to_write++] = 0x01;
	}
	//check if it is off or changed
	if ((modes != 2) && (ctx->test_pattern == 2)) {
		_i2c_data[_size_to_write++] = 0x5000;
		_i2c_data[_size_to_write++] = 0xCB;//11001011
		_i2c_data[_size_to_write++] = 0x5001;
		_i2c_data[_size_to_write++] = 0x43;//01000011
		_i2c_data[_size_to_write++] = 0x5002;
		_i2c_data[_size_to_write++] = 0x9E;//10011110
		/* need check with vendor */
		_i2c_data[_size_to_write++] = 0x5081;
		_i2c_data[_size_to_write++] = 0x0;
	} else if ((modes != 5) && (ctx->test_pattern == 5)) {
		//@@ Solid color BLACK - off
		//6c 3019 d2
		//6c 4308 00
		_i2c_data[_size_to_write++] = 0x3019;
		_i2c_data[_size_to_write++] = 0xd2;
		_i2c_data[_size_to_write++] = 0x4308;
		_i2c_data[_size_to_write++] = 0x00;
	}
	if (_size_to_write > 0) {
		ov13b10lz_table_write_cmos_sensor(ctx,
			_i2c_data,
			_size_to_write);
	}
	ctx->test_pattern = modes;
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	UINT32 temperature = 0;
	INT32 temperature_convert = 0;

	/*TEMP_SEN_CTL */
	write_cmos_sensor(ctx,0x4d12, 0x01);
	temperature = (read_cmos_sensor(ctx,0x4d13) << 8) |
		read_cmos_sensor(ctx,0x4d13);
	if (temperature < 0xc000)
		temperature_convert = temperature / 256;
	else
		temperature_convert = 191 - temperature / 256;

	if (temperature_convert > 191)
		temperature_convert = 191;
	else if (temperature_convert < -64)
		temperature_convert = -64;

	return 20;
}

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	cam_pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable)
		write_cmos_sensor(ctx,0x0100, 0x01);
	else
		write_cmos_sensor(ctx,0x0100, 0x00);

	mdelay(10);

	return ERROR_NONE;
}

static int feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	UINT32 rate;

	cam_pr_debug("feature_id = %d\n", feature_id);

	switch (feature_id) {
	// fuzr to do
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
	if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
		*(feature_data + 0) =
			sizeof(ov13b10lz_ana_gain_table);
	} else {
		memcpy((void *)(uintptr_t) (*(feature_data + 1)),
		(void *)ov13b10lz_ana_gain_table,
		sizeof(ov13b10lz_ana_gain_table));
	}
		break; 
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(ctx,
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
	break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = ctx->line_length;
		*feature_return_para_16 = ctx->frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*feature_return_para_32 = 1; /* NON */
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*feature_return_para_32 = 2; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		cam_pr_debug("SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO \n");
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
	    *feature_return_para_32 = ctx->pclk;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_ESHUTTER:
	    set_shutter(ctx, *feature_data);
	break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	    night_mode(ctx, (BOOL) * feature_data);
	break;
	case SENSOR_FEATURE_SET_GAIN:
	    set_gain(ctx, (UINT16) * feature_data);
	break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
	break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(ctx,sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
	    sensor_reg_data->RegData =
			read_cmos_sensor(ctx, sensor_reg_data->RegAddr);
	break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
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
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
	break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	    get_default_framerate_by_scenario(ctx,
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
	break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		cam_pr_debug("SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO \n");
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_debug("Please use EEPROM function\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (UINT32)*feature_data);
	break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	    *feature_return_para_32 = imgsensor_info.checksum_value;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_FRAMERATE:
	    ctx->current_fps = *feature_data_32;
		cam_pr_debug("current fps :%d\n", ctx->current_fps);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		cam_pr_debug("GET_CROP_INFO scenarioId:%d\n",
			*feature_data_32);

	    wininfo = (struct  SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		}
	break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		cam_pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
	    ihdr_write_shutter_gain(ctx, (UINT16)*feature_data,
			(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
	break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
			(uintptr_t)(*(feature_data+1));



		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			break;
		}
		break;

	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		break;

	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature(ctx);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(ctx, KAL_FALSE);
		break;

	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		cam_pr_debug("SENSOR_FEATURE_GET_MIPI_PIXEL_RATE \n");
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			rate = imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			rate = imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			rate = imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			rate = imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			rate = imgsensor_info.custom1.mipi_pixel_rate;
			break;	
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
		break;
/*	case SENSOR_FEATURE_GET_PIXEL_RATE:
	{
		kal_uint32 rate;

		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			rate = (imgsensor_info.cap.pclk /
					(imgsensor_info.cap.linelength - 80))*
					imgsensor_info.cap.grabwindow_width;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			rate = (imgsensor_info.normal_video.pclk /
					(imgsensor_info.normal_video.linelength - 80))*
					imgsensor_info.normal_video.grabwindow_width;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			rate = (imgsensor_info.hs_video.pclk /
					(imgsensor_info.hs_video.linelength - 80))*
					imgsensor_info.hs_video.grabwindow_width;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			rate = (imgsensor_info.slim_video.pclk /
					(imgsensor_info.slim_video.linelength - 80))*
					imgsensor_info.slim_video.grabwindow_width;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			rate = (imgsensor_info.pre.pclk /
					(imgsensor_info.pre.linelength - 80))*
					imgsensor_info.pre.grabwindow_width;
			break;
		}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	}
		break;*/
	default:
	break;
	}

	return ERROR_NONE;
}   /*  feature_control(ctx)  */

#ifdef IMGSENSOR_VC_ROUTING
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4208,
			.vsize = 3120,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4208,
			.vsize = 3120,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x01f0,
			.vsize = 0x05d8,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x01f0,
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
			.hsize = 0x0500,
			.vsize = 0x02d0,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0a28,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4208,//0x0fa0,
			.vsize = 3120,//0x08ca,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cap);
		memcpy(fd->entry, frame_desc_cap, sizeof(frame_desc_cap));
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
		fd->num_entries = ARRAY_SIZE(frame_desc_cus1);
		memcpy(fd->entry, frame_desc_cus1, sizeof(frame_desc_cus1));
		break;

	default:
		return -1;
	}

	return 0;
}
#endif

static const struct subdrv_ctx defctx = {

	.ana_gain_def = 4 * BASEGAIN,
	.ana_gain_max = 15.5 * BASEGAIN,
	.ana_gain_min = BASEGAIN,
	.ana_gain_step = 4,
	.exposure_def = 0x3D0,
	.exposure_max = 0xffffe9 - 22,
	.exposure_min = 4,
	.exposure_step = 1,
	.frame_time_delay_frame = 3,
	.margin = 22,
	.max_frame_length = 0xffffe9,
	.is_streaming = KAL_FALSE,

	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,
	.gain = 4 * BASEGAIN,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	//.ihdr_en = 0,
	.i2c_write_id = 0x6c,
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
	//csi_param->cphy_settle = 0x1b;
	//csi_param->cphy_settle = 98;

	csi_param->dphy_trail = 76;
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
			{HW_ID_RST, 0, 1},
			{HW_ID_AFVDD, 2800000, 0},
			{HW_ID_AVDD, 2800000, 0},
			{HW_ID_DVDD, 1200000, 5},
			{HW_ID_DOVDD, 1800000, 0},
			{HW_ID_RST, 1, 5},
			{HW_ID_MCLK_DRIVING_CURRENT, 8, 0},
};

const struct subdrv_entry ov13b10lz_mipi_raw_entry = {
	.name = "ov13b10lz_mipi_raw",
	.id = OV13B10LZ_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

