// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   s5k5e9yxmipiraw_Sensor.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
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

#include "s5k5e9yxmipiraw_Sensor.h"
#include "s5k5e9yx_ana_gain_table.h"
#include "s5k5e9yx_Sensor_setting.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"
#include "adaptor.h"

#define SENSOR_NAME  "s5k5e9yx"
#define DEBUG_LOG_EN 0
#define I2C_WRITE_TABLE   1

#define PFX "S5K5E9YX_camera_sensor"
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_DEBUG(...) do { if ((DEBUG_LOG_EN)) LOG_INF(__VA_ARGS__); } while (0)

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor_16(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor_16(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define s5k5e9yx_table_write_cmos_sensor_8(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)
#define s5k5e9yx_table_write_cmos_sensor_16(...) subdrv_i2c_wr_regs_u16(__VA_ARGS__)
#define s5k5e9yx_burst_write_cmos_sensor_8(...) subdrv_i2c_wr_p8(__VA_ARGS__)
#define s5k5e9yx_burst_write_cmos_sensor_16(...) subdrv_i2c_wr_p16(__VA_ARGS__)

#define S5K5E9YX_EEPROM_READ_ID  0xA0
#define S5K5E9YX_EEPROM_WRITE_ID 0xA1
#define GAIN_DEFAULT 0x0100

#define _I2C_BUF_SIZE 256
static kal_uint16 _i2c_data[_I2C_BUF_SIZE];
static unsigned int _size_to_write;

static void commit_write_sensor(struct subdrv_ctx *ctx)
{
    if (_size_to_write) {
        s5k5e9yx_table_write_cmos_sensor_16(ctx, _i2c_data, _size_to_write);
        memset(_i2c_data, 0x0, sizeof(_i2c_data));
        _size_to_write = 0;
    }
}

static void set_cmos_sensor_16(struct subdrv_ctx *ctx,
            kal_uint16 reg, kal_uint16 val)
{
    if (_size_to_write > _I2C_BUF_SIZE - 2)
        commit_write_sensor(ctx);

    _i2c_data[_size_to_write++] = reg;
    _i2c_data[_size_to_write++] = val;
}

static struct imgsensor_info_struct imgsensor_info = {
        .sensor_id = S5K5E9YX_SENSOR_ID,

        .checksum_value = 0xB1F1B3CC,

        .pre = {
            .pclk = 190000000,
            .linelength = 3112,
            .framelength = 2030,
            .startx = 0,
            .starty = 0,
            .grabwindow_width = 1296,
            .grabwindow_height = 972,
            .mipi_data_lp2hs_settle_dc = 85,
            .mipi_pixel_rate = 175200000,//269400000
            .max_framerate = 300,
        },
        .cap = {
            .pclk =190000000,
            .linelength = 3112,
            .framelength = 2030, //2035: 30.0019
            .startx =0,
            .starty = 0,
            .grabwindow_width = 2592,
            .grabwindow_height = 1944,
            .mipi_data_lp2hs_settle_dc = 85,
            .mipi_pixel_rate = 175200000,//269400000
            .max_framerate = 300,
        },
        .normal_video = {
            .pclk =190000000,
            .linelength = 3112,
            .framelength = 2030,
            .startx =0,
            .starty = 0,
            .grabwindow_width = 2592,
            .grabwindow_height = 1460,
            .mipi_data_lp2hs_settle_dc = 85,
            .mipi_pixel_rate = 175200000,//269400000
            .max_framerate = 300,
        },
        .hs_video = {
            .pclk = 190000000,
            .linelength = 3112,
            .framelength =544,
            .startx = 0,
            .starty = 0,
            .grabwindow_width = 640,
            .grabwindow_height = 480,
            .mipi_data_lp2hs_settle_dc = 85,
            .mipi_pixel_rate = 175200000,
            .max_framerate = 1200,
        },
        .slim_video = {
            .pclk = 190000000,
            .linelength = 3112,
            .framelength =2034,
            .startx = 0,
            .starty = 0,
            .grabwindow_width = 1280,
            .grabwindow_height = 720,
            .mipi_data_lp2hs_settle_dc = 85,
            .mipi_pixel_rate = 175200000,
            .max_framerate = 300,
        },

        .margin = 4,
        .min_shutter = 4,
        .min_gain = BASEGAIN, /*1x gain*/
        .max_gain = BASEGAIN * 16, /*16x gain*/
        .min_gain_iso = 100,
        .exp_step = 1,
        .gain_step = 32,
        .gain_type = 2,
        .max_frame_length = 0xfffc,//0xffff-3,
        .ae_shut_delay_frame = 0,
        .ae_sensor_gain_delay_frame = 0,
        .ae_ispGain_delay_frame = 2,
        .ihdr_support = 0,/*1, support; 0,not support*/
        .ihdr_le_firstline = 0,/*1,le first; 0, se first*/
        .sensor_mode_num = 4,/*support sensor mode num*/

        .cap_delay_frame = 3,
        .pre_delay_frame = 3,
        .video_delay_frame = 3,
        .hs_video_delay_frame = 3,
        .slim_video_delay_frame = 3,

        .isp_driving_current = ISP_DRIVING_4MA,
        .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
        .mipi_sensor_type = MIPI_OPHY_NCSI2,
        .mipi_settle_delay_mode = 1,  //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
        .sensor_output_dataformat =
                SENSOR_OUTPUT_FORMAT_RAW_Gr,
        .mclk = 24,
        .mipi_lane_num = SENSOR_MIPI_2_LANE,
        .i2c_addr_table = {0x5a,0xff},
        .i2c_speed = 4000,
};

/* Sensor output window information */
/*no mirror flip*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
    {2592, 1944, 0, 0, 2592, 1944, 1296, 972, 0000, 0000, 1296, 972, 0, 0, 1296, 972},      /* Preview */
    {2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944},   /* capture */
    {2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944},   /* video */
    {2592, 1944, 24, 260, 2560, 1440, 640, 480, 0000, 0000, 640, 480, 0, 0, 640, 480},      /* hight speed video */
    {2592, 1944, 24, 20,  2560, 1920, 1280, 720, 0000, 0000, 1280, 720, 0, 0, 1280, 720},   /* slim speed video */
};

static void set_dummy(struct subdrv_ctx *ctx)
{
    DEBUG_LOG(ctx, "dummyline = %d, dummypixels = %d\n",
        ctx->dummy_line, ctx->dummy_pixel);
    write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
    write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
    write_cmos_sensor_8(ctx, 0x0342, ctx->line_length >> 8);
    write_cmos_sensor_8(ctx, 0x0343, ctx->line_length & 0xFF);

    //commit_write_sensor(ctx);
}   /*  set_dummy  */

static void set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate,
    kal_bool min_framelength_en)
{
    kal_uint32 frame_length = ctx->frame_length;

    LOG_DEBUG("framerate = %d, min framelength should enable %d\n",
        framerate, min_framelength_en);

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
}   /*  set_max_framerate  */

static void write_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
    kal_uint16 realtime_fps = 0;

    if (shutter > ctx->min_frame_length - imgsensor_info.margin)
        ctx->frame_length = shutter + imgsensor_info.margin;
    else
        ctx->frame_length = ctx->min_frame_length;

    if (ctx->frame_length > imgsensor_info.max_frame_length)
        ctx->frame_length = imgsensor_info.max_frame_length;
    if (shutter < imgsensor_info.min_shutter)
        shutter = imgsensor_info.min_shutter;

    if (ctx->autoflicker_en) {
        realtime_fps =
            ctx->pclk / ctx->line_length
            * 10 / ctx->frame_length;
        if (realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(ctx, 296, 0);
        else if (realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(ctx, 146, 0);
    }
    /* Update Shutter*/
    //set_cmos_sensor_16(ctx, 0x0104, 0x01);//gph start
    write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
    write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
    write_cmos_sensor_8(ctx, 0x0202, shutter >> 8);
    write_cmos_sensor_8(ctx, 0x0203, shutter & 0xFF);
    //set_cmos_sensor_16(ctx, 0x0202, shutter);
    //if (!ctx->ae_ctrl_gph_en)
        //set_cmos_sensor_16(ctx, 0x0104, 0x00);//grouphold end
//  commit_write_sensor(ctx);

    DEBUG_LOG(ctx, "shutter = %d, framelength = %d\n",
        shutter, ctx->frame_length);
}   /*  write_shutter  */

/*
 ************************************************************************
 * FUNCTION
 *  set_shutter
 *
 * DESCRIPTION
 *  This function set e-shutter of sensor to change exposure time.0xFF
 *  iShutter : exposured lines
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */
static void set_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
    ctx->shutter = shutter;

    write_shutter(ctx, shutter);
}   /*  set_shutter */

static void set_frame_length(struct subdrv_ctx *ctx, kal_uint16 frame_length)
{
    if (frame_length > 1)
        ctx->frame_length = frame_length;

    if (ctx->frame_length > imgsensor_info.max_frame_length)
        ctx->frame_length = imgsensor_info.max_frame_length;
    if (ctx->min_frame_length > ctx->frame_length)
        ctx->frame_length = ctx->min_frame_length;

    /* Extend frame length */
    //write_cmos_sensor_16(ctx, 0x0340, ctx->frame_length & 0xFFFF);

    write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
    write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);

    LOG_DEBUG("Framelength: set=%d/input=%d/min=%d\n",
        ctx->frame_length, frame_length, ctx->min_frame_length);
}

static void set_shutter_frame_length(struct subdrv_ctx *ctx,
    kal_uint16 shutter, kal_uint16 frame_length)
{   kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;

    ctx->shutter = shutter;

    /* Change frame time */
    if (frame_length > 1)
        dummy_line = frame_length - ctx->frame_length;

    ctx->frame_length =
        ctx->frame_length + dummy_line;


    if (shutter > ctx->frame_length - imgsensor_info.margin)
        ctx->frame_length = shutter + imgsensor_info.margin;


    if (ctx->frame_length > imgsensor_info.max_frame_length)
        ctx->frame_length = imgsensor_info.max_frame_length;


    shutter =
        (shutter < imgsensor_info.min_shutter)
        ? imgsensor_info.min_shutter
        : shutter;
    shutter =
        (shutter >
        (imgsensor_info.max_frame_length - imgsensor_info.margin))
        ? (imgsensor_info.max_frame_length - imgsensor_info.margin)
        : shutter;

    if (ctx->autoflicker_en) {
        realtime_fps =
            ctx->pclk / ctx->line_length *
            10 / ctx->frame_length;
        if (realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(ctx, 296, 0);
        else if (realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(ctx, 146, 0);
    }

    /* Update Shutter */
    set_cmos_sensor_16(ctx, 0x0340, ctx->frame_length & 0xFFFF);
    set_cmos_sensor_16(ctx, 0X0202, shutter & 0xFFFF);

    commit_write_sensor(ctx);

    LOG_INF("shutter = %d, framelength = %d/%d, dummy_line= %d\n",
        shutter, ctx->frame_length,
        frame_length, dummy_line);

}

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
    kal_uint16 reg_gain = 0x0;

    //reg_gain = gain * 32 / BASEGAIN;
     reg_gain = gain/2;
    return (kal_uint16)reg_gain;
}

/*
 ************************************************************************
 * FUNCTION
 *  set_gain
 *
 * DESCRIPTION
 *  This function is to set global gain to sensor.
 *
 * PARAMETERS
 *  iGain : sensor global gain(base: 0x400)
 *
 * RETURNS
 *  the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */
static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{
    kal_uint16 reg_gain;

    if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
        LOG_INF("Error gain setting");

        if (gain < imgsensor_info.min_gain)
            gain = imgsensor_info.min_gain;
        else if (gain > imgsensor_info.max_gain)
            gain = imgsensor_info.max_gain;
    }

    reg_gain = gain2reg(ctx, gain);
    ctx->gain = reg_gain;
    DEBUG_LOG(ctx, "gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

    set_cmos_sensor_16(ctx, 0x0204, reg_gain);
    if (ctx->ae_ctrl_gph_en)
        set_cmos_sensor_16(ctx, 0x0104, 0x00); //grouphold end
    commit_write_sensor(ctx);
    return gain;
}   /*  set_gain  */

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
    int timeout = ctx->current_fps ? (10000 / ctx->current_fps) + 1 : 101;
    int i = 0;
    int framecnt = 0;

    LOG_INF("streaming_enable(0= Sw Standby,1= streaming): %d\n", enable);
    if (enable) {
        write_cmos_sensor_8(ctx, 0x0100, 0X01);
        mDELAY(10);
    } else {
        write_cmos_sensor_8(ctx, 0x0100, 0x00); 
        for (i = 0; i < timeout; i++) {
            mDELAY(5);
            framecnt = read_cmos_sensor_8(ctx, 0x0005);
            if (framecnt == 0xFF) {
                LOG_DEBUG("Stream Off OK at i=%d.\n", i);
                return ERROR_NONE;
            }
        }
        LOG_INF("Stream Off Fail! framecnt= %d.\n", framecnt);
    }
    return ERROR_NONE;
}

static void S5K5E9yx_read_otp(struct subdrv_ctx *ctx)
{
    kal_uint8 module_house_id,sensor_id,lens_id,year,month,ir_filter_id;

	module_house_id = read_cmos_sensor_8(ctx, 0x0A05);
	      sensor_id = read_cmos_sensor_8(ctx, 0x0A09);
	        lens_id = read_cmos_sensor_8(ctx, 0x0A0A);
	           year = read_cmos_sensor_8(ctx, 0x0A0C);
		  month = read_cmos_sensor_8(ctx, 0x0A0D);
	   ir_filter_id = read_cmos_sensor_8(ctx, 0x0A10);
		LOG_INF("S5K5E9yx_read_otp() module_house_id=%x\n",module_house_id);
		LOG_INF("S5K5E9yx_read_otp() sensor_id=%x\n",sensor_id);
	        LOG_INF("S5K5E9yx_read_otp() year=%x,month%x\n",year,month);
	        LOG_INF("S5K5E9yx_read_otp() ir_filter_id%x\n",ir_filter_id);
}

static void S5K5E9yx_test_ok_otp(struct subdrv_ctx *ctx)
{
    int a1,a2,b1,b2,c1,c2,d1,d2;

//AWB OTP test,BT data
//	write_cmos_sensor_8(ctx,0x3C0F, 0x00);
//	write_cmos_sensor_16(ctx, 0x020E, 0xFFFF);
//	write_cmos_sensor_16(ctx, 0x0210, 0x0000);
//	write_cmos_sensor_16(ctx, 0x0212, 0xFFFF);
//	write_cmos_sensor_16(ctx, 0x0214, 0x0000);

//test s5k5e9 write okey
	a1 = read_cmos_sensor_8(ctx,0x020E);
	a2 = read_cmos_sensor_8(ctx,0x020F);
	b1 = read_cmos_sensor_8(ctx,0x0210);
	b2 = read_cmos_sensor_8(ctx,0x0211);
	c1 = read_cmos_sensor_8(ctx,0x0212);
	c2 = read_cmos_sensor_8(ctx,0x0213);
	d1 = read_cmos_sensor_8(ctx,0x0214);
	d2 = read_cmos_sensor_8(ctx,0x0215);
        LOG_INF(" S5K5E9_apply_otp() a1 =%x,a2 =%x\n",a1,a2);
        LOG_INF(" S5K5E9_apply_otp() b1 =%x,b2=%x\n",b1,b2);
	LOG_INF(" S5K5E9_apply_otp() c1 =%x,c2=%x\n",c1,c2);
	LOG_INF(" S5K5E9_apply_otp() d1 =%x,d2=%x\n",d1,d2);

}

/* s5k5e9yx apply OTP :LSC  AWB  */

static kal_uint32 S5K5E9_apply_otp(struct subdrv_ctx *ctx)
{
	int iPage,otp_flag1,otp_flag2,otp_flag3;
	int rg,bg,gg,r_ratio,b_ratio,R_GAIN,B_GAIN,G_GAIN,Gb_GAIN,Gr_GAIN,S5K5E9_RG_Ratio_Typical,S5K5E9_BG_Ratio_Typical,S5K5E9_GrGb_Ratio_Typical;
	int iHigh_RG,iLow_RG,iHigh_BG,iLow_BG,iHigh_GG,iLow_GG,iHigh_RG1,iLow_RG1,iHigh_BG1,iLow_BG1,iHigh_GG1,iLow_GG1;

	write_cmos_sensor_8(ctx, 0x0100, 0x00);   //steam off
	write_cmos_sensor_8(ctx, 0x3400, 0x00);
	write_cmos_sensor_8(ctx, 0x0B01, 0x01);
	mdelay(500);

	write_cmos_sensor_8(ctx, 0x0100, 0x01);   //steam on
	mdelay(50);

	write_cmos_sensor_8(ctx, 0x0A02, 0x11);	  //set page
	write_cmos_sensor_8(ctx, 0x3B41, 0x01);
	write_cmos_sensor_8(ctx, 0x3B42, 0x03);
	write_cmos_sensor_8(ctx, 0x3B40, 0x01);
	write_cmos_sensor_8(ctx, 0x0A00, 0x01);
	mdelay(5);

	otp_flag1=read_cmos_sensor_8(ctx, 0x0A04);

	write_cmos_sensor_8(ctx, 0x0A00, 0x04);
	write_cmos_sensor_8(ctx, 0x0A00, 0x00);
	write_cmos_sensor_8(ctx, 0x0A02, 0x12);
	write_cmos_sensor_8(ctx, 0x3B41, 0x01);
	write_cmos_sensor_8(ctx, 0x3B42, 0x03);
	write_cmos_sensor_8(ctx, 0x3B40, 0x01);
	write_cmos_sensor_8(ctx, 0x0A00, 0x01);
	mdelay(5);

	otp_flag2=read_cmos_sensor_8(ctx, 0x0A04);
	write_cmos_sensor_8(ctx, 0x0A00, 0x04);
	write_cmos_sensor_8(ctx, 0x0A00, 0x00);
	write_cmos_sensor_8(ctx, 0x0A02, 0x13);
	write_cmos_sensor_8(ctx, 0x3B41, 0x01);
	write_cmos_sensor_8(ctx, 0x3B42, 0x03);
	write_cmos_sensor_8(ctx, 0x3B40, 0x01);
	write_cmos_sensor_8(ctx, 0x0A00, 0x01);
	mdelay(5);

	otp_flag3=read_cmos_sensor_8(ctx, 0x0A04);
	write_cmos_sensor_8(ctx, 0x0A00, 0x04);
	write_cmos_sensor_8(ctx, 0x0A00, 0x00);

	if ((otp_flag1 & 0xC0) == 0x40)
	{
	   iPage = 17;
	}
	else if ((otp_flag2 & 0xC0) == 0x40)
	{
	   iPage = 18;
	}
	else if ((otp_flag3 & 0xC0) == 0x40)
	{
	   iPage = 19;
	}
	else
	{
	    LOG_ERR("[*******s5k5e9yx read OTP ERROR**********]: %s,%d,E\n",PFX, __func__,__LINE__);
	    return ERROR_NONE;
	}

	write_cmos_sensor_8(ctx,0x0100, 0x01);

	mdelay(50);
	write_cmos_sensor_8(ctx,0x0A02, iPage);
	write_cmos_sensor_8(ctx,0x3B41, 0x01);
	write_cmos_sensor_8(ctx,0x3B42, 0x03);
	write_cmos_sensor_8(ctx,0x3B40, 0x01);

	write_cmos_sensor_8(ctx, 0x0A00, 0x01);
	mdelay(5);

        S5K5E9yx_read_otp(ctx);
	iHigh_RG = read_cmos_sensor_8(ctx, 0x0A19);
	iLow_RG = read_cmos_sensor_8(ctx, 0x0A1A);

	iHigh_BG = read_cmos_sensor_8(ctx, 0x0A1B);
	iLow_BG = read_cmos_sensor_8(ctx, 0x0A1C);

	iHigh_GG = read_cmos_sensor_8(ctx, 0x0A1D);
	iLow_GG = read_cmos_sensor_8(ctx, 0x0A1E);

	iHigh_RG1 = read_cmos_sensor_8(ctx, 0x0A27);
	iLow_RG1 = read_cmos_sensor_8(ctx, 0x0A28);

	iHigh_BG1 = read_cmos_sensor_8(ctx, 0x0A29);
	iLow_BG1 = read_cmos_sensor_8(ctx, 0x0A2A);

	iHigh_GG1 = read_cmos_sensor_8(ctx, 0x0A2B);
	iLow_GG1 = read_cmos_sensor_8(ctx, 0x0A2C);

	write_cmos_sensor_8(ctx,0x0A00, 0x04);
	write_cmos_sensor_8(ctx,0x0A00, 0x00);

	S5K5E9_RG_Ratio_Typical = (((iHigh_RG << 8)&0xff00) + (iLow_RG));
	S5K5E9_BG_Ratio_Typical = (((iHigh_BG << 8)&0xff00) + (iLow_BG));
	S5K5E9_GrGb_Ratio_Typical = (((iHigh_GG << 8)&0xff00) + (iLow_GG));

	rg = (((iHigh_RG1 << 8)&0xff00) + (iLow_RG1));
	bg = (((iHigh_BG1 << 8)&0xff00) + (iLow_BG1));
	gg = (((iHigh_GG1 << 8)&0xff00) + (iLow_GG1));

	r_ratio = 512 * (S5K5E9_RG_Ratio_Typical) /(rg);
	b_ratio = 512 * (S5K5E9_BG_Ratio_Typical) /(bg);

	if(r_ratio >= 512 )
	   {
		if(b_ratio>=512)
		{
			R_GAIN = (GAIN_DEFAULT * r_ratio / 512);
			G_GAIN = GAIN_DEFAULT;
			B_GAIN = (GAIN_DEFAULT * b_ratio / 512);
		}
		else
		{
			R_GAIN =  (GAIN_DEFAULT * r_ratio / b_ratio);
			G_GAIN = (GAIN_DEFAULT * 512 / b_ratio);
			B_GAIN = GAIN_DEFAULT;
		}
	        } else {

		if(b_ratio >= 512)
		{
			R_GAIN = GAIN_DEFAULT;
			G_GAIN =(GAIN_DEFAULT * 512 / r_ratio);
			B_GAIN =(GAIN_DEFAULT *  b_ratio / r_ratio);
		} else {

			Gr_GAIN = (GAIN_DEFAULT * 512 / r_ratio );
			Gb_GAIN = (GAIN_DEFAULT * 512 / b_ratio );

			if(Gr_GAIN >= Gb_GAIN)
			{
				R_GAIN = GAIN_DEFAULT;
				G_GAIN = (GAIN_DEFAULT * 512 / r_ratio );
				B_GAIN = (GAIN_DEFAULT * b_ratio / r_ratio);
			} else {

				R_GAIN = (GAIN_DEFAULT * r_ratio / b_ratio );
				G_GAIN = (GAIN_DEFAULT * 512 / b_ratio );
				B_GAIN = GAIN_DEFAULT;
			}
		}
	}

	write_cmos_sensor_8(ctx,0x3C0F, 0x00);
	write_cmos_sensor_16(ctx, 0x020E, G_GAIN);
	write_cmos_sensor_16(ctx, 0x0210, R_GAIN);
	write_cmos_sensor_16(ctx, 0x0212, B_GAIN);
	write_cmos_sensor_16(ctx, 0x0214, G_GAIN);

	S5K5E9yx_test_ok_otp(ctx);
	LOG_INF("S5K5E9_apply_otp() R_GAIN =%x\n",R_GAIN );
	LOG_INF("S5K5E9_apply_otp() G_GAIN=%x\n",G_GAIN);
	LOG_INF("S5K5E9_apply_otp() B_GAIN=%x\n",B_GAIN);
	LOG_ERR("[*******s5k5e9yx add OTP End**********]: %s,%d,E\n",PFX, __func__,__LINE__);

	return ERROR_NONE;
}


static void sensor_init(struct subdrv_ctx *ctx)
{
#ifndef I2C_WRITE_TABLE
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__); 
    write_cmos_sensor_8(ctx,0x0100, 0x00);  //gobal
    write_cmos_sensor_8(ctx,0x3B45, 0x01);
    write_cmos_sensor_8(ctx,0x0B05, 0x01);
    write_cmos_sensor_8(ctx,0x392F, 0x01);
    write_cmos_sensor_8(ctx,0x3930, 0x00);
    write_cmos_sensor_8(ctx,0x3924, 0x7F);
    write_cmos_sensor_8(ctx,0x3925, 0xFD);
    write_cmos_sensor_8(ctx,0x3C08, 0xFF);
    write_cmos_sensor_8(ctx,0x3C09, 0xFF);
    write_cmos_sensor_8(ctx,0x3C31, 0xFF);
    write_cmos_sensor_8(ctx,0x3C32, 0xFF);
    write_cmos_sensor_8(ctx,0x3290, 0x10);
    write_cmos_sensor_8(ctx,0x3200, 0x01);
    write_cmos_sensor_8(ctx,0x3074, 0x06);
    write_cmos_sensor_8(ctx,0x3075, 0x2F);
    write_cmos_sensor_8(ctx,0x308A, 0x20);
    write_cmos_sensor_8(ctx,0x308B, 0x08);
    write_cmos_sensor_8(ctx,0x308C, 0x0B);
    write_cmos_sensor_8(ctx,0x3081, 0x07);
    write_cmos_sensor_8(ctx,0x307B, 0x85);
    write_cmos_sensor_8(ctx,0x307A, 0x0A);
    write_cmos_sensor_8(ctx,0x3079, 0x0A);
    write_cmos_sensor_8(ctx,0x306E, 0x71);
    write_cmos_sensor_8(ctx,0x306F, 0x28);
    write_cmos_sensor_8(ctx,0x301F, 0x20);
    write_cmos_sensor_8(ctx,0x3012, 0x4E);
    write_cmos_sensor_8(ctx,0x306B, 0x9A);
    write_cmos_sensor_8(ctx,0x3091, 0x16);
    write_cmos_sensor_8(ctx,0x30C4, 0x06);
    write_cmos_sensor_8(ctx,0x306A, 0x79);
    write_cmos_sensor_8(ctx,0x30B0, 0xFF);
    write_cmos_sensor_8(ctx,0x306D, 0x08);
    write_cmos_sensor_8(ctx,0x3084, 0x16);
    write_cmos_sensor_8(ctx,0x3070, 0x0F);
    write_cmos_sensor_8(ctx,0x30C2, 0x05);
    write_cmos_sensor_8(ctx,0x3069, 0x87);
    write_cmos_sensor_8(ctx,0x3C0F, 0x00);
   // write_cmos_sensor_8(ctx,0x0A02,   0x3F);
    write_cmos_sensor_8(ctx,0x3080, 0x08); //04
    write_cmos_sensor_8(ctx,0x3083, 0x14);
    write_cmos_sensor_8(ctx,0x3400, 0x01);
    write_cmos_sensor_8(ctx,0x3C34, 0xEA);
    write_cmos_sensor_8(ctx,0x3C35, 0x5C);
   // write_cmos_sensor_8(ctx,0x0B00, 0x00);
#else
    int i = 0;
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__); 
    LOG_ERR("[%s]%s,%d sizeof init array: %d, E\n",PFX, __func__,__LINE__,ARRAY_SIZE(init_setting_array)); 
    for(i = 0; i < ARRAY_SIZE(init_setting_array); i++) {
        write_cmos_sensor_8(ctx,init_setting_array[i].addr, init_setting_array[i].data);
    }

#endif
    LOG_ERR("[%s]%s,%d X\n",PFX, __func__,__LINE__); 

}   /*  sensor_init  */

static void preview_setting(struct subdrv_ctx *ctx)
{   
#ifndef I2C_WRITE_TABLE
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__); 
     write_cmos_sensor_8(ctx,0x0136,    0x18);
     write_cmos_sensor_8(ctx,0x0137,    0x00);
     write_cmos_sensor_8(ctx,0x0305,    0x04);
     write_cmos_sensor_8(ctx,0x0306,    0x00);
     write_cmos_sensor_8(ctx,0x0307,    0x5F);
     write_cmos_sensor_8(ctx,0x030D,    0x04);
     write_cmos_sensor_8(ctx,0x030E,    0x00);
     write_cmos_sensor_8(ctx,0x030F,    0x92);
     write_cmos_sensor_8(ctx,0x3C1F,    0x00);
     write_cmos_sensor_8(ctx,0x3C17,    0x00);
     write_cmos_sensor_8(ctx,0x0112,    0x0A);
     write_cmos_sensor_8(ctx,0x0113,    0x0A);
     write_cmos_sensor_8(ctx,0x0114,    0x01);
     write_cmos_sensor_8(ctx,0x0820,    0x03);
     write_cmos_sensor_8(ctx,0x0821,    0x6C);
     write_cmos_sensor_8(ctx,0x0822,    0x00);
     write_cmos_sensor_8(ctx,0x0823,    0x00);
     write_cmos_sensor_8(ctx,0x3929,    0x0F);
     write_cmos_sensor_8(ctx,0x0344,    0x00);
     write_cmos_sensor_8(ctx,0x0345,    0x08);
     write_cmos_sensor_8(ctx,0x0346,    0x00);
     write_cmos_sensor_8(ctx,0x0347,    0x08);
     write_cmos_sensor_8(ctx,0x0348,    0x0A);
     write_cmos_sensor_8(ctx,0x0349,    0x27);
     write_cmos_sensor_8(ctx,0x034A,    0x07);
     write_cmos_sensor_8(ctx,0x034B,    0x9F);
     write_cmos_sensor_8(ctx,0x034C,    0x0A);
     write_cmos_sensor_8(ctx,0x034D,    0x20);
     write_cmos_sensor_8(ctx,0x034E,    0x07);
     write_cmos_sensor_8(ctx,0x034F,    0x98);
     write_cmos_sensor_8(ctx,0x0900,    0x00);
     write_cmos_sensor_8(ctx,0x0901,    0x00);
     write_cmos_sensor_8(ctx,0x0381,    0x01);
     write_cmos_sensor_8(ctx,0x0383,    0x01);
     write_cmos_sensor_8(ctx,0x0385,    0x01);
     write_cmos_sensor_8(ctx,0x0387,    0x01);
     write_cmos_sensor_8(ctx,0x0101,    0x00);
     write_cmos_sensor_8(ctx,0x0340,    0x07);
     write_cmos_sensor_8(ctx,0x0341,    0xF4);
     write_cmos_sensor_8(ctx,0x0342,    0x0C);
     write_cmos_sensor_8(ctx,0x0343,    0x28);
     write_cmos_sensor_8(ctx,0x0200,    0x0B);
     write_cmos_sensor_8(ctx,0x0201,    0x9C);
     write_cmos_sensor_8(ctx,0x0202,    0x00);
     write_cmos_sensor_8(ctx,0x0203,    0x02);
     write_cmos_sensor_8(ctx,0x30B8,    0x2E);
     write_cmos_sensor_8(ctx,0x30BA,    0x36);
     LOG_ERR("[%s]%s,%d X\n",PFX, __func__,__LINE__); 
#else
    int i = 0;
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__); 
    for(i = 0; i < ARRAY_SIZE(preview_setting_array); i++) {
        write_cmos_sensor_8(ctx,preview_setting_array[i].addr, preview_setting_array[i].data);
    }

#endif
}   /*  preview_setting  */

static void capture_setting(struct subdrv_ctx *ctx)
{
#ifndef I2C_WRITE_TABLE
     LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__); 
     write_cmos_sensor_8(ctx,0x0136,    0x18);
     write_cmos_sensor_8(ctx,0x0137,    0x00);
     write_cmos_sensor_8(ctx,0x0305,    0x04);
     write_cmos_sensor_8(ctx,0x0306,    0x00);
     write_cmos_sensor_8(ctx,0x0307,    0x5F);
     write_cmos_sensor_8(ctx,0x030D,    0x04);
     write_cmos_sensor_8(ctx,0x030E,    0x00);
     write_cmos_sensor_8(ctx,0x030F,    0x92);
     write_cmos_sensor_8(ctx,0x3C1F,    0x00);
     write_cmos_sensor_8(ctx,0x3C17,    0x00);
     write_cmos_sensor_8(ctx,0x0112,    0x0A);
     write_cmos_sensor_8(ctx,0x0113,    0x0A);
     write_cmos_sensor_8(ctx,0x0114,    0x01);
     write_cmos_sensor_8(ctx,0x0820,    0x03);
     write_cmos_sensor_8(ctx,0x0821,    0x6C);
     write_cmos_sensor_8(ctx,0x0822,    0x00);
     write_cmos_sensor_8(ctx,0x0823,    0x00);
     write_cmos_sensor_8(ctx,0x3929,    0x0F);
     write_cmos_sensor_8(ctx,0x0344,    0x00);
     write_cmos_sensor_8(ctx,0x0345,    0x08);
     write_cmos_sensor_8(ctx,0x0346,    0x00);
     write_cmos_sensor_8(ctx,0x0347,    0x08);
     write_cmos_sensor_8(ctx,0x0348,    0x0A);
     write_cmos_sensor_8(ctx,0x0349,    0x27);
     write_cmos_sensor_8(ctx,0x034A,    0x07);
     write_cmos_sensor_8(ctx,0x034B,    0x9f);
     write_cmos_sensor_8(ctx,0x034C,    0x0A);
     write_cmos_sensor_8(ctx,0x034D,    0x20);
     write_cmos_sensor_8(ctx,0x034E,    0x07);
     write_cmos_sensor_8(ctx,0x034F,    0x98);
     write_cmos_sensor_8(ctx,0x0900,    0x00);
     write_cmos_sensor_8(ctx,0x0901,    0x00);
     write_cmos_sensor_8(ctx,0x0381,    0x01);
     write_cmos_sensor_8(ctx,0x0383,    0x01);
     write_cmos_sensor_8(ctx,0x0385,    0x01);
     write_cmos_sensor_8(ctx,0x0387,    0x01);
     write_cmos_sensor_8(ctx,0x0101,    0x00);
     write_cmos_sensor_8(ctx,0x0340,    0x09);
     write_cmos_sensor_8(ctx,0x0341,    0xEF);
     write_cmos_sensor_8(ctx,0x0342,    0x0C);
     write_cmos_sensor_8(ctx,0x0343,    0x28);
     write_cmos_sensor_8(ctx,0x0200,    0x0B);
     write_cmos_sensor_8(ctx,0x0201,    0x9C);
     write_cmos_sensor_8(ctx,0x0202,    0x00);
     write_cmos_sensor_8(ctx,0x0203,    0x02);
     write_cmos_sensor_8(ctx,0x30B8,    0x2E);
     write_cmos_sensor_8(ctx,0x30BA,    0x36); 
     write_cmos_sensor_8(ctx,0x0100,    0x01);
#else
    int i = 0;
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__); 
    for(i = 0; i < ARRAY_SIZE(capture_setting_array); i++) {
        write_cmos_sensor_8(ctx,capture_setting_array[i].addr, capture_setting_array[i].data);
    }

#endif

    LOG_ERR("[%s]%s,%d X\n",PFX, __func__,__LINE__);
}   /*  capture_setting  */

static void normal_video_setting(struct subdrv_ctx *ctx)
{
#ifndef I2C_WRITE_TABLE
      LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__);
      write_cmos_sensor_8(ctx,0x0136,   0x18);
      write_cmos_sensor_8(ctx,0x0137,   0x00);
      write_cmos_sensor_8(ctx,0x0305,   0x04);
      write_cmos_sensor_8(ctx,0x0306,   0x00);
      write_cmos_sensor_8(ctx,0x0307,   0x5F);
      write_cmos_sensor_8(ctx,0x030D,   0x04);
      write_cmos_sensor_8(ctx,0x030E,   0x00);
      write_cmos_sensor_8(ctx,0x030F,   0x92);
      write_cmos_sensor_8(ctx,0x3C1F,   0x00);
      write_cmos_sensor_8(ctx,0x3C17,   0x00);
      write_cmos_sensor_8(ctx,0x0112,   0x0A);
      write_cmos_sensor_8(ctx,0x0113,   0x0A);
      write_cmos_sensor_8(ctx,0x0114,   0x01);
      write_cmos_sensor_8(ctx,0x0820,   0x03);
      write_cmos_sensor_8(ctx,0x0821,   0x6C);
      write_cmos_sensor_8(ctx,0x0822,   0x00);
      write_cmos_sensor_8(ctx,0x0823,   0x00);
      write_cmos_sensor_8(ctx,0x3929,   0x0F);
      write_cmos_sensor_8(ctx,0x0344,   0x00);
      write_cmos_sensor_8(ctx,0x0345,   0x08);
      write_cmos_sensor_8(ctx,0x0346,   0x00);
      write_cmos_sensor_8(ctx,0x0347,   0x08);
      write_cmos_sensor_8(ctx,0x0348,   0x0A);
      write_cmos_sensor_8(ctx,0x0349,   0x27);
      write_cmos_sensor_8(ctx,0x034A,   0x07);
      write_cmos_sensor_8(ctx,0x034B,   0x9f);
      write_cmos_sensor_8(ctx,0x034C,   0x0A);
      write_cmos_sensor_8(ctx,0x034D,   0x20);
      write_cmos_sensor_8(ctx,0x034E,   0x07);
      write_cmos_sensor_8(ctx,0x034F,   0x98);
      write_cmos_sensor_8(ctx,0x0900,   0x00);
      write_cmos_sensor_8(ctx,0x0901,   0x00);
      write_cmos_sensor_8(ctx,0x0381,   0x01);
      write_cmos_sensor_8(ctx,0x0383,   0x01);
      write_cmos_sensor_8(ctx,0x0385,   0x01);
      write_cmos_sensor_8(ctx,0x0387,   0x01);
      write_cmos_sensor_8(ctx,0x0101,   0x00);
      write_cmos_sensor_8(ctx,0x0340,   0x07);
      write_cmos_sensor_8(ctx,0x0341,   0xEE);
      write_cmos_sensor_8(ctx,0x0342,   0x0C);
      write_cmos_sensor_8(ctx,0x0343,   0x28);
      write_cmos_sensor_8(ctx,0x0200,   0x0B);
      write_cmos_sensor_8(ctx,0x0201,   0x9C);
      write_cmos_sensor_8(ctx,0x0202,   0x00);
      write_cmos_sensor_8(ctx,0x0203,   0x02);
      write_cmos_sensor_8(ctx,0x30B8,   0x2E);
      write_cmos_sensor_8(ctx,0x30BA,   0x36);
#else
    int i = 0;
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__);
    for(i = 0; i < ARRAY_SIZE(normal_video_setting_array); i++) {
        write_cmos_sensor_8(ctx,normal_video_setting_array[i].addr, normal_video_setting_array[i].data);
    }

#endif
    LOG_ERR("[%s]%s,%d X\n",PFX, __func__,__LINE__);
}   /*  normal_video_setting  */

static void hs_video_setting(struct subdrv_ctx *ctx)
{
#ifndef I2C_WRITE_TABLE
      LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__);
      write_cmos_sensor_8(ctx,0x0136,   0x18);
      write_cmos_sensor_8(ctx,0x0137,   0x00);
      write_cmos_sensor_8(ctx,0x0305,   0x04);
      write_cmos_sensor_8(ctx,0x0306,   0x00);
      write_cmos_sensor_8(ctx,0x0307,   0x5F);
      write_cmos_sensor_8(ctx,0x030D,   0x04);
      write_cmos_sensor_8(ctx,0x030E,   0x00);
      write_cmos_sensor_8(ctx,0x030F,   0x92);
      write_cmos_sensor_8(ctx,0x3C1F,   0x00);
      write_cmos_sensor_8(ctx,0x3C17,   0x00);
      write_cmos_sensor_8(ctx,0x0112,   0x0A);
      write_cmos_sensor_8(ctx,0x0113,   0x0A);
      write_cmos_sensor_8(ctx,0x0114,   0x01);
      write_cmos_sensor_8(ctx,0x0820,   0x03);
      write_cmos_sensor_8(ctx,0x0821,   0x6C);
      write_cmos_sensor_8(ctx,0x0822,   0x00);
      write_cmos_sensor_8(ctx,0x0823,   0x00);
      write_cmos_sensor_8(ctx,0x3929,   0x0F);
      write_cmos_sensor_8(ctx,0x0344,   0x00);
      write_cmos_sensor_8(ctx,0x0345,   0x18);
      write_cmos_sensor_8(ctx,0x0346,   0x00);
      write_cmos_sensor_8(ctx,0x0347,   0x14);
      write_cmos_sensor_8(ctx,0x0348,   0x0A);
      write_cmos_sensor_8(ctx,0x0349,   0x17);
      write_cmos_sensor_8(ctx,0x034A,   0x07);
      write_cmos_sensor_8(ctx,0x034B,   0x93);
      write_cmos_sensor_8(ctx,0x034C,   0x02);
      write_cmos_sensor_8(ctx,0x034D,   0x80);
      write_cmos_sensor_8(ctx,0x034E,   0x01);
      write_cmos_sensor_8(ctx,0x034F,   0xE0);
      write_cmos_sensor_8(ctx,0x0900,   0x01);
      write_cmos_sensor_8(ctx,0x0901,   0x44);
      write_cmos_sensor_8(ctx,0x0381,   0x01);
      write_cmos_sensor_8(ctx,0x0383,   0x01);
      write_cmos_sensor_8(ctx,0x0385,   0x01);
      write_cmos_sensor_8(ctx,0x0387,   0x07);
      write_cmos_sensor_8(ctx,0x0101,   0x00);
      write_cmos_sensor_8(ctx,0x0340,   0x02);
      write_cmos_sensor_8(ctx,0x0341,   0x20);
      write_cmos_sensor_8(ctx,0x0342,   0x0C);
      write_cmos_sensor_8(ctx,0x0343,   0x28);
      write_cmos_sensor_8(ctx,0x0200,   0x0B);
      write_cmos_sensor_8(ctx,0x0201,   0x9C);
      write_cmos_sensor_8(ctx,0x0202,   0x00);
      write_cmos_sensor_8(ctx,0x0203,   0x02);
      write_cmos_sensor_8(ctx,0x30B8,   0x2E);
      write_cmos_sensor_8(ctx,0x30BA,   0x36);
#else
    int i = 0;
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__);
    for(i = 0; i < ARRAY_SIZE(hs_video_setting_array); i++) {
        write_cmos_sensor_8(ctx,hs_video_setting_array[i].addr, hs_video_setting_array[i].data);
    }

#endif  
     LOG_ERR("[%s]%s,%d X\n",PFX, __func__,__LINE__);
}   /*  hs_video_setting  */

static void slim_video_setting(struct subdrv_ctx *ctx)
{
#ifndef I2C_WRITE_TABLE
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__);
    write_cmos_sensor_8(ctx,0x0136, 0x18);
    write_cmos_sensor_8(ctx,0x0137, 0x00);
    write_cmos_sensor_8(ctx,0x0305, 0x04);
    write_cmos_sensor_8(ctx,0x0306, 0x00);
    write_cmos_sensor_8(ctx,0x0307, 0x5F);
    write_cmos_sensor_8(ctx,0x030D, 0x04);
    write_cmos_sensor_8(ctx,0x030E, 0x00);
    write_cmos_sensor_8(ctx,0x030F, 0x92);
    write_cmos_sensor_8(ctx,0x3C1F, 0x00);
    write_cmos_sensor_8(ctx,0x3C17, 0x00);
    write_cmos_sensor_8(ctx,0x0112, 0x0A);
    write_cmos_sensor_8(ctx,0x0113, 0x0A);
    write_cmos_sensor_8(ctx,0x0114, 0x01);
    write_cmos_sensor_8(ctx,0x0820, 0x03);
    write_cmos_sensor_8(ctx,0x0821, 0x6C);
    write_cmos_sensor_8(ctx,0x0822, 0x00);
    write_cmos_sensor_8(ctx,0x0823, 0x00);
    write_cmos_sensor_8(ctx,0x3929, 0x0F);
    write_cmos_sensor_8(ctx,0x0344, 0x00);
    write_cmos_sensor_8(ctx,0x0345, 0x18);
    write_cmos_sensor_8(ctx,0x0346, 0x01);
    write_cmos_sensor_8(ctx,0x0347, 0x04);
    write_cmos_sensor_8(ctx,0x0348, 0x0A);
    write_cmos_sensor_8(ctx,0x0349, 0x17);
    write_cmos_sensor_8(ctx,0x034A, 0x06);
    write_cmos_sensor_8(ctx,0x034B, 0xA3);
    write_cmos_sensor_8(ctx,0x034C, 0x05);
    write_cmos_sensor_8(ctx,0x034D, 0x00);
    write_cmos_sensor_8(ctx,0x034E, 0x02);
    write_cmos_sensor_8(ctx,0x034F, 0xD0);
    write_cmos_sensor_8(ctx,0x0900, 0x01);
    write_cmos_sensor_8(ctx,0x0901, 0x22);
    write_cmos_sensor_8(ctx,0x0381, 0x01);
    write_cmos_sensor_8(ctx,0x0383, 0x01);
    write_cmos_sensor_8(ctx,0x0385, 0x01);
    write_cmos_sensor_8(ctx,0x0387, 0x03);
    write_cmos_sensor_8(ctx,0x0101, 0x00);
    write_cmos_sensor_8(ctx,0x0340, 0x07);
    write_cmos_sensor_8(ctx,0x0341, 0xF2);
    write_cmos_sensor_8(ctx,0x0342, 0x0C);
    write_cmos_sensor_8(ctx,0x0343, 0x28);
    write_cmos_sensor_8(ctx,0x0200, 0x0B);
    write_cmos_sensor_8(ctx,0x0201, 0x9C);
    write_cmos_sensor_8(ctx,0x0202, 0x00);
    write_cmos_sensor_8(ctx,0x0203, 0x02);
    write_cmos_sensor_8(ctx,0x30B8, 0x2A);
    write_cmos_sensor_8(ctx,0x30BA, 0x2E);
#else
    int i = 0;
    LOG_ERR("[%s]%s,%d E\n",PFX, __func__,__LINE__);
    for(i = 0; i < ARRAY_SIZE(slim_video_setting_array); i++) {
        write_cmos_sensor_8(ctx,slim_video_setting_array[i].addr, slim_video_setting_array[i].data);
    }

#endif

    LOG_ERR("[%s]%s,%d X\n",PFX, __func__,__LINE__);
}   /*  slim_video_setting  */


/*************************************************************************
 * FUNCTION
 *  get_imgsensor_id
 *
 * DESCRIPTION
 *  This function get the sensor ID
 *
 * PARAMETERS
 *  *sensorID : return the sensor ID
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;

    LOG_ERR("[s5k5e9yx]%s,%d", __func__,__LINE__);
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
        do {
            *sensor_id =
                ((read_cmos_sensor_8(ctx, 0x0000) << 8) | read_cmos_sensor_8(ctx, 0x0001));
                
            LOG_ERR("read out sensor id 0x%x\n",
                *sensor_id);
            if (*sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
                    ctx->i2c_write_id, *sensor_id);
                return ERROR_NONE;
            }
            LOG_ERR("Read sensor id fail, id: 0x%x\n",
                ctx->i2c_write_id);
            retry--;
        } while (retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id !=  imgsensor_info.sensor_id) {
/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF*/
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *  open
 *
 * DESCRIPTION
 *  This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *  None
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************
 */
static int open(struct subdrv_ctx *ctx)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint16 sensor_id = 0;
    LOG_ERR("[s5k5e9yx]: %s,%d,E\n",__func__,__LINE__);
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
        do {
            sensor_id = ((read_cmos_sensor_8(ctx, 0x0000) << 8) | read_cmos_sensor_8(ctx, 0x0001));
            if (sensor_id == imgsensor_info.sensor_id) {
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
    if (imgsensor_info.sensor_id !=  sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init(ctx);
	/* s5k5e9yx apply OTP :LSC  AWB  */

	S5K5E9_apply_otp(ctx);


    ctx->autoflicker_en = KAL_FALSE;
    ctx->sensor_mode = IMGSENSOR_MODE_INIT;
    ctx->shutter = 0x3D0;
    ctx->gain = BASEGAIN * 4;
    ctx->pclk = imgsensor_info.pre.pclk;
    ctx->frame_length = imgsensor_info.pre.framelength;
    ctx->line_length = imgsensor_info.pre.linelength;
    ctx->min_frame_length = imgsensor_info.pre.framelength;
    ctx->dummy_pixel = 0;
    ctx->dummy_line = 0;
    ctx->ihdr_mode = 0;
    ctx->test_pattern = 0;
    ctx->current_fps = imgsensor_info.pre.max_framerate;

    return ERROR_NONE;
}   /*  open  */

/*************************************************************************
 * FUNCTION
 *  close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *  None
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int close(struct subdrv_ctx *ctx)
{
    streaming_control(ctx, KAL_FALSE);

    LOG_INF("E\n");
    return ERROR_NONE;
}   /*  close  */

/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *  This function start the sensor preview.
 *
 * PARAMETERS
 *  *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *  None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(struct subdrv_ctx *ctx,
        MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_ERR("%s,%d: E\n",__func__,__LINE__);

    ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
    ctx->pclk = imgsensor_info.pre.pclk;
    ctx->line_length = imgsensor_info.pre.linelength;
    ctx->frame_length = imgsensor_info.pre.framelength;
    ctx->min_frame_length = imgsensor_info.pre.framelength;
    ctx->autoflicker_en = KAL_FALSE;

    preview_setting(ctx);

    LOG_ERR("%s,%d: X\n",__func__,__LINE__);
    return ERROR_NONE;
}   /*  preview   */

/*************************************************************************
 * FUNCTION
 *  capture
 *
 * DESCRIPTION
 *  This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *  None
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
    ctx->pclk = imgsensor_info.cap.pclk;
    ctx->line_length = imgsensor_info.cap.linelength;
    ctx->frame_length = imgsensor_info.cap.framelength;
    ctx->min_frame_length = imgsensor_info.cap.framelength;
    ctx->autoflicker_en = KAL_FALSE;

    capture_setting(ctx);

    LOG_INF("X\n");
    return ERROR_NONE;
} /* capture(ctx) */

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

    normal_video_setting(ctx);

    LOG_INF("X\n");
    return ERROR_NONE;
}   /*  normal_video   */

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

    hs_video_setting(ctx);

    LOG_INF("X\n");
    return ERROR_NONE;
}   /*  hs_video   */

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

    slim_video_setting(ctx);

    LOG_INF("X\n");
    return ERROR_NONE;
}   /*  slim_video   */

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

static int get_info(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
        MSDK_SENSOR_INFO_STRUCT *sensor_info,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_DEBUG("scenario_id = %d\n", scenario_id);

    sensor_info->SensorClockPolarity =
        SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity =
        SENSOR_CLOCK_POLARITY_LOW;
    /* not use */
    sensor_info->SensorHsyncPolarity =
        SENSOR_CLOCK_POLARITY_LOW;
    /* inverse with datasheet*/
    sensor_info->SensorVsyncPolarity =
        SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType =
        imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType =
        imgsensor_info.mipi_sensor_type;
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

    sensor_info->SensorMasterClockSwitch = 0;
    /* not use */
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

    sensor_info->SensorWidthSampling = 0;   /* 0 is default 1x*/
    sensor_info->SensorHightSampling = 0;   /* 0 is default 1x*/
    sensor_info->SensorPacketECCOrder = 1;

    return ERROR_NONE;
}   /*  get_info  */

static int control(struct subdrv_ctx *ctx,
        enum MSDK_SCENARIO_ID_ENUM scenario_id,
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
    default:
        LOG_INF("Error ScenarioId setting");
        preview(ctx, image_window, sensor_config_data);
        return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}   /* control(ctx) */

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
    LOG_DEBUG("framerate = %d\n ", framerate);
    if (framerate == 0) {
        /* Dynamic frame rate*/
        return ERROR_NONE;
    }
    if ((framerate == 300) &&
            (ctx->autoflicker_en == KAL_TRUE))
        ctx->current_fps = 296;
    else if ((framerate == 150) &&
            (ctx->autoflicker_en == KAL_TRUE))
        ctx->current_fps = 146;
    else
        ctx->current_fps = framerate;
    set_max_framerate(ctx, ctx->current_fps, 1);
    set_dummy(ctx);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx, kal_bool enable, UINT16 framerate)
{
    (void) framerate;

    LOG_DEBUG("enable = %d\n", enable);
    if (enable) {/*enable auto flicker*/
        ctx->autoflicker_en = KAL_TRUE;
    } else {/*Cancel Auto flick*/
        ctx->autoflicker_en = KAL_FALSE;
    }
    return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
    enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    DEBUG_LOG(ctx, "scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
    case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        frame_length = imgsensor_info.pre.pclk /
            framerate * 10 /
            imgsensor_info.pre.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.pre.framelength) ?
            (frame_length - imgsensor_info.pre.framelength) : 0;
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
            (frame_length >
                imgsensor_info.normal_video.framelength) ?
            (frame_length -
                imgsensor_info.normal_video.framelength)
            : 0;
        ctx->frame_length =
            imgsensor_info.normal_video.framelength
            + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        if (ctx->frame_length > ctx->shutter)
            set_dummy(ctx);
        break;
    case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        if (ctx->current_fps != imgsensor_info.cap.max_framerate) {
            LOG_DEBUG("Warning: current_fps %d fps is not support",
                framerate);
            LOG_DEBUG("so use cap's setting: %d fps!\n",
                imgsensor_info.cap.max_framerate / 10);
        }
        frame_length = imgsensor_info.cap.pclk /
            framerate * 10 /
            imgsensor_info.cap.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.cap.framelength) ?
            (frame_length - imgsensor_info.cap.framelength) : 0;
        ctx->frame_length =
            imgsensor_info.cap.framelength
            + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        if (ctx->frame_length > ctx->shutter)
            set_dummy(ctx);
        break;
    case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
        frame_length = imgsensor_info.hs_video.pclk /
            framerate * 10 /
            imgsensor_info.hs_video.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.hs_video.framelength) ?
            (frame_length - imgsensor_info.hs_video.framelength) :
            0;
        ctx->frame_length =
            imgsensor_info.hs_video.framelength
            + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        if (ctx->frame_length > ctx->shutter)
            set_dummy(ctx);
        break;
    case SENSOR_SCENARIO_ID_SLIM_VIDEO:
        frame_length = imgsensor_info.slim_video.pclk /
            framerate * 10 /
            imgsensor_info.slim_video.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.slim_video.framelength) ?
            (frame_length - imgsensor_info.slim_video.framelength) :
            0;
        ctx->frame_length =
            imgsensor_info.slim_video.framelength
            + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        if (ctx->frame_length > ctx->shutter)
            set_dummy(ctx);
        break;
    default:/*coding with  preview scenario by default*/
        frame_length = imgsensor_info.pre.pclk /
            framerate * 10 /
            imgsensor_info.pre.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.pre.framelength) ?
            (frame_length - imgsensor_info.pre.framelength) : 0;
        ctx->frame_length =
            imgsensor_info.pre.framelength + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        if (ctx->frame_length > ctx->shutter)
            set_dummy(ctx);
        LOG_INF("error scenario_id = %d, we use preview scenario\n",
            scenario_id);
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
    enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_DEBUG("scenario_id = %d\n", scenario_id);

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
    default:
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_uint32 mode)
{
    if (mode != ctx->test_pattern)
        pr_debug("mode: %d\n", mode);

    if (mode)
        write_cmos_sensor_16(ctx, 0x0600, mode); /*100% Color bar*/
    else if (ctx->test_pattern)
        write_cmos_sensor_16(ctx, 0x0600, 0x0000); /*No pattern*/

    ctx->test_pattern = mode;
    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_data(struct subdrv_ctx *ctx, struct mtk_test_pattern_data *data)
{

    DEBUG_LOG(ctx, "test_patterndata mode = %d  R = %x, Gr = %x,Gb = %x,B = %x\n",
        ctx->test_pattern,
        data->Channel_R >> 22, data->Channel_Gr >> 22,
        data->Channel_Gb >> 22, data->Channel_B >> 22);

    set_cmos_sensor_16(ctx, 0x0602, (data->Channel_R >> 22) & 0x3ff);
    //set_cmos_sensor(ctx, 0x0603, (data->Channel_R >> 22) & 0xff);
    set_cmos_sensor_16(ctx, 0x0604, (data->Channel_Gr >> 22) & 0x3ff);
    //set_cmos_sensor(ctx, 0x0605, (data->Channel_Gr >> 22) & 0xff);
    set_cmos_sensor_16(ctx, 0x0606, (data->Channel_B >> 22) & 0x3ff);
    //set_cmos_sensor(ctx, 0x0607, (data->Channel_B >> 22) & 0xff);
    set_cmos_sensor_16(ctx, 0x0608, (data->Channel_Gb >> 22) & 0x3ff);
    //set_cmos_sensor(ctx, 0x0609, (data->Channel_Gb >> 22) & 0xff);
    commit_write_sensor(ctx);
    return ERROR_NONE;
}

static int feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
    UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
    UINT16 *feature_data_16 = (UINT16 *) feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
    UINT32 *feature_data_32 = (UINT32 *) feature_para;
    unsigned long long *feature_data = (unsigned long long *) feature_para;
    //char *data = (char *)(uintptr_t)(*(feature_data + 1));
   // UINT16 type = (UINT16)(*feature_data);

    struct SET_PD_BLOCK_INFO_T *PDAFinfo;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
        (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

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
        case SENSOR_SCENARIO_ID_CUSTOM4:
        case SENSOR_SCENARIO_ID_CUSTOM5:
            *(feature_data + 1)
            = (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
                imgsensor_info.sensor_output_dataformat;
            break;
        }
    break;
    case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
        if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
            *(feature_data + 0) =
                sizeof(s5k5e9yx_ana_gain_table);
        } else {
            memcpy((void *)(uintptr_t) (*(feature_data + 1)),
            (void *)s5k5e9yx_ana_gain_table,
            sizeof(s5k5e9yx_ana_gain_table));
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
    case SENSOR_FEATURE_GET_MAX_EXP_LINE:
        *(feature_data + 2) =
            imgsensor_info.max_frame_length - imgsensor_info.margin;
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
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.pre.pclk;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = 2500000;
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
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
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
        /*night_mode(ctx, (BOOL) *feature_data);*/
        break;
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
        /* get the lens driver ID from EEPROM */
        /* or just return LENS_DRIVER_ID_DO_NOT_CARE */
        /* if EEPROM does not exist in camera module.*/
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
            *(feature_data + 1));
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        get_default_framerate_by_scenario(ctx,
            (enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
            (MUINT32 *)(uintptr_t)(*(feature_data + 1)));
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        set_test_pattern_mode(ctx, (UINT32)*feature_data);
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN_DATA:
        set_test_pattern_data(ctx, (struct mtk_test_pattern_data *)feature_data);
        break;
    /*for factory mode auto testing*/
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        *feature_return_para_32 = imgsensor_info.checksum_value;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        ctx->current_fps = *feature_data_32;
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_DEBUG("SENSOR_FEATURE_GET_CROP_INFO, scenarioId:%d\n",
            *feature_data_32);
        wininfo =
            (struct SENSOR_WINSIZE_INFO_STRUCT *)
            (uintptr_t)(*(feature_data + 1));
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
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        default:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[0],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_INFO:
        LOG_DEBUG("SENSOR_FEATURE_GET_PDAF_INFO, scenarioId:%lld\n",
            *feature_data);
        PDAFinfo =
            (struct SET_PD_BLOCK_INFO_T *)
            (uintptr_t)(*(feature_data + 1));

        switch (*feature_data) {
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
        case SENSOR_SCENARIO_ID_SLIM_VIDEO:
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        default:
            break;
        }
        break;
    case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
        LOG_DEBUG("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY, scenarioId:%lld\n",
            *feature_data);
        switch (*feature_data) {
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = 0;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = 0; /* video & capture use same setting*/
            break;
        case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = 0;
            break;
        case SENSOR_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = 0;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = 0;
            break;
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = 0;
            break;
        }
        break;
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
        set_shutter_frame_length(ctx, (UINT16) *feature_data,
            (UINT16) *(feature_data + 1));
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        /* margin info by scenario */
        *(feature_data + 2) = imgsensor_info.margin;
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND");
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
        case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
        case SENSOR_SCENARIO_ID_SLIM_VIDEO:
            *feature_return_para_32 = 4;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        default:
            *feature_return_para_32 = 1;
            break;
        }
        LOG_DEBUG("SENSOR_FEATURE_GET_BINNING_TYPE, AE_binning_type:%d\n",
            *feature_return_para_32);
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        {
            kal_uint32 rate;

            switch (*feature_data) {
            case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
                rate =
                imgsensor_info.cap.mipi_pixel_rate;
                break;
            case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
                rate =
                imgsensor_info.normal_video.mipi_pixel_rate;
                break;
            case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
                rate =
                imgsensor_info.hs_video.mipi_pixel_rate;
                break;
            case SENSOR_SCENARIO_ID_SLIM_VIDEO:
                rate =
                imgsensor_info.slim_video.mipi_pixel_rate;
                break;
            case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
                rate =
                    imgsensor_info.pre.mipi_pixel_rate;
                break;
            default:
                    rate = 0;
                    break;
            }
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
        }
        break;
    case SENSOR_FEATURE_SET_FRAMELENGTH:
        set_frame_length(ctx, (UINT16) (*feature_data));
        break;
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
            .hsize = 1296,
            .vsize = 972,
        },
    },
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 2592,
            .vsize = 1944,
        },
    },
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 2592,
            .vsize = 1460,
        },
    },
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 2592,
            .vsize = 1460,
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
    case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
    case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
        fd->num_entries = ARRAY_SIZE(frame_desc_vid);
        memcpy(fd->entry, frame_desc_vid, sizeof(frame_desc_vid));
        break;
    case SENSOR_SCENARIO_ID_SLIM_VIDEO:
        fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
        fd->num_entries = ARRAY_SIZE(frame_desc_slim_vid);
        memcpy(fd->entry, frame_desc_slim_vid, sizeof(frame_desc_slim_vid));
        break;
    default:
        return -1;
    }
    return 0;
}
#endif

static const struct subdrv_ctx defctx = {

    .ana_gain_def = BASEGAIN * 4,
    .ana_gain_max = BASEGAIN * 16,
    .ana_gain_min = BASEGAIN,
    .ana_gain_step = 32,
    .exposure_def = 0x3D0,
    .exposure_max = 0xfffc - 3,
    .exposure_min = 3,
    .exposure_step = 1,
    .margin = 3,
    .max_frame_length = 0xfffc,

    .mirror = IMAGE_NORMAL, //mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3D0,   /*current shutter*/
    .gain = BASEGAIN * 4,           /*current gain*/
    .dummy_pixel = 0,       /*current dummypixel*/
    .dummy_line = 0,        /*current dummyline*/
    .current_fps = 0,
    /*full size current fps : 24fps for PIP, 30fps for Normal or ZSD*/
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,

    .current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
    .ihdr_mode = 0, /*sensor need support LE, SE with HDR feature*/
    .i2c_write_id = 0x5a,
    .ae_ctrl_gph_en = 0,
};

static int init_ctx(struct subdrv_ctx *ctx,
        struct i2c_client *i2c_client, u8 i2c_write_id)
{
    LOG_INF("%s, enter",__func__);
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
    csi_param->dphy_trail = 76;
    //csi_param->cphy_settle = 76;

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
    .get_csi_param = get_csi_param,
#ifdef IMGSENSOR_VC_ROUTING
    .get_frame_desc = get_frame_desc,
#endif
};

static struct subdrv_pw_seq_entry pw_seq[] = {
    {HW_ID_MCLK, 24, 0},
    {HW_ID_MCLK_DRIVING_CURRENT, 2, 0},
    {HW_ID_RST,  0, 1},
    {HW_ID_DVDD, 1200000, 1},
    {HW_ID_AVDD, 2800000, 1},
    {HW_ID_DOVDD, 1800000, 3},
    {HW_ID_RST, 1, 2}
};

const struct subdrv_entry s5k5e9yx_mipi_raw_entry = {
    .name = "s5k5e9yx_mipi_raw",
    .id = S5K5E9YX_SENSOR_ID,
    .pw_seq = pw_seq,
    .pw_seq_cnt = ARRAY_SIZE(pw_seq),
    .ops = &ops,
};
