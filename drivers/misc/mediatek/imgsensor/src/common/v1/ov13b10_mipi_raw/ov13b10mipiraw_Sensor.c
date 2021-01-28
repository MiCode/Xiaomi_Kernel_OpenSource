// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "ov13b10_camera_sensor"
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

#include "ov13b10mipiraw_Sensor.h"
#define cam_pr_debug(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)

#define MULTI_WRITE 1
static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV13B10_SENSOR_ID,

	.checksum_value = 0x3acb7e3a,

	.pre = {
		.pclk = 112000000,
		.linelength =  1176,
		.framelength = 3196,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 19,//unit , ns , 85
		.mipi_pixel_rate = 224000000,
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
		.mipi_data_lp2hs_settle_dc = 19,
		.mipi_pixel_rate = 448000000,
		.max_framerate = 300,
	},
	/*size@24fps, same as capture*/
	.cap1 = {
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 6392,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 19,
		.mipi_pixel_rate = 448000000,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 112000000,
		.linelength =  1176,
		.framelength = 3196,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 19,//unit , ns , 85
		.mipi_pixel_rate = 224000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 108250560,
		.linelength = 1122,
		.framelength = 804,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 90,
		.mipi_pixel_rate = 224000000,
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 108250560,
		.linelength =  1122,
		.framelength = 3216,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1024,
		.grabwindow_height = 768,
		.mipi_data_lp2hs_settle_dc = 90,
		.mipi_pixel_rate = 224000000,
		.max_framerate = 1200,
	},

	.margin = 8,
	.min_shutter = 0x04,
	.max_frame_length = 0x7fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,

	.cap_delay_frame = 3,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

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

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,
	.gain = 0x100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x6c,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{ 4240, 3136,    0,    0, 4240, 3136, 2120, 1560,
	      8,    4, 2104, 1560,    0,    0, 2104, 1560},
	{ 4240, 3136,    0,    0, 4240, 3136, 4240, 3136,
	      16,    8, 4208, 3120,    0,    0, 4208, 3120},
	{ 4240, 3136,    0,    0, 4240, 3136, 1920, 1080,
	      0,    0, 1920, 1080,    0,    0, 1920, 1080},
	/*{ 4256, 3168,    0,    8, 4256, 3152, 4256, 3152,
	    16,    8, 4224, 3136,    0,    0, 4224, 3136},
		*/
	{ 4256, 3168,   64,   64, 4128, 3104, 1032,  776,
	   196,  148,  640,  480,    0,    0,  640,  480},
	{ 4256, 3168,   64,   64, 4128, 3104, 1032,  776,
	     4,    4, 1024,  768,    0,    0, 1024,  768},
};

/*PD information update*/
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	 .i4OffsetX = 0,
	 .i4OffsetY = 0,
	 .i4PitchX = 32,
	 .i4PitchY = 32,
	 .i4PairNum = 8,
	 .i4SubBlkW = 16,
	 .i4SubBlkH = 8,
	 .i4PosL = {{14, 6}, {30, 6}, {6, 10}, {22, 10},
		{14, 22}, {30, 22}, {6, 26}, {22, 26} },
	 .i4PosR = {{14, 2}, {30, 2}, {6, 14}, {22, 14},
		{14, 18}, {30, 18}, {6, 30}, {22, 30} },
	 .iMirrorFlip = 0,
	 .i4BlockNumX = 132,
	 .i4BlockNumY = 98,
};

#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3
#endif

static kal_uint16 ov13b10_table_write_cmos_sensor(
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

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pusendcmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
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

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;

	/* frame_length and shutter should be an even number. */
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	if (imgsensor.autoflicker_en == KAL_TRUE) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			realtime_fps = 296;
			set_max_framerate(realtime_fps, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			realtime_fps = 146;
			set_max_framerate(realtime_fps, 0);
		} else	{
			imgsensor.frame_length =
				(imgsensor.frame_length  >> 1) << 1;
			write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x380f,
				imgsensor.frame_length & 0xFF);
		}
	} else	{
		imgsensor.frame_length = (imgsensor.frame_length  >> 1) << 1;
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	/*Warning : shutter must be even. Odd might happen Unexpected Results */
	write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);
	write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);
	write_cmos_sensor(0x3502, (shutter<<4)  & 0xF0);

	cam_pr_debug("shutter =%d, framelength =%d, realtime_fps =%d\n",
		shutter, imgsensor.frame_length, realtime_fps);
}

static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
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

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	unsigned long flags;

	reg_gain = gain2reg(gain);
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.gain = reg_gain;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	cam_pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	write_cmos_sensor(0x03508, (reg_gain >> 8));
	write_cmos_sensor(0x03509, (reg_gain&0xff));

	return gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
}

static void night_mode(kal_bool enable)
{
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_ov13b10[] = {
	0x0103, 0x01,
	0x0303, 0x01,
	0x0305, 0x46,
	0x0321, 0x00,
	0x0323, 0x04,
	0x0324, 0x01,
	0x0325, 0x50,
	0x0326, 0x81,
	0x0327, 0x04,
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
	0x5049, 0xa4
};
#endif

static void sensor_init(void)
{
	write_cmos_sensor(0x0103, 0x01);
	mdelay(10);
#if MULTI_WRITE
	cam_pr_debug("MULTI_WRITE\n");
	ov13b10_table_write_cmos_sensor(
		addr_data_pair_init_ov13b10,
		sizeof(addr_data_pair_init_ov13b10) / sizeof(kal_uint16));
#else
	cam_pr_debug("E\n");
	write_cmos_sensor(0x0300, 0x02);
	write_cmos_sensor(0x0301, 0x00);
	write_cmos_sensor(0x0302, 0x5a);

	write_cmos_sensor(0x0304, 0x00);
	write_cmos_sensor(0x0305, 0x01);

	write_cmos_sensor(0x3022, 0x01);
	write_cmos_sensor(0x3012, 0x40);
	write_cmos_sensor(0x3013, 0x72);
	write_cmos_sensor(0x3021, 0x23);
	write_cmos_sensor(0x484b, 0x07);
	write_cmos_sensor(0x3016, 0x72);
	write_cmos_sensor(0x301b, 0xf0);
	write_cmos_sensor(0x301f, 0xd0);
	write_cmos_sensor(0x3106, 0x15);
	write_cmos_sensor(0x3107, 0x23);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x3508, 0x02);
	write_cmos_sensor(0x3509, 0x00);
	write_cmos_sensor(0x350a, 0x00);
	write_cmos_sensor(0x350e, 0x00);
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x02);
	write_cmos_sensor(0x3512, 0x00);
	write_cmos_sensor(0x3600, 0x2b);
	write_cmos_sensor(0x3601, 0x52);
	write_cmos_sensor(0x3602, 0x60);
	write_cmos_sensor(0x3612, 0x05);
	write_cmos_sensor(0x3613, 0xa4);
	write_cmos_sensor(0x3620, 0x80);
	write_cmos_sensor(0x3621, 0x08);
	write_cmos_sensor(0x3622, 0x30);
	write_cmos_sensor(0x3624, 0x1c);
	write_cmos_sensor(0x3661, 0x80);
	write_cmos_sensor(0x3664, 0x73);
	write_cmos_sensor(0x3665, 0xa7);
	write_cmos_sensor(0x366e, 0xff);
	write_cmos_sensor(0x366f, 0xf4);
	write_cmos_sensor(0x3674, 0x00);
	write_cmos_sensor(0x3679, 0x0c);
	write_cmos_sensor(0x367f, 0x01);
	write_cmos_sensor(0x3680, 0x0c);
	write_cmos_sensor(0x3681, 0x60);
	write_cmos_sensor(0x3682, 0x17);
	write_cmos_sensor(0x3683, 0xa9);
	write_cmos_sensor(0x3684, 0x9a);
	write_cmos_sensor(0x3706, 0x40);
	write_cmos_sensor(0x3709, 0x68);
	write_cmos_sensor(0x371a, 0x3e);
	write_cmos_sensor(0x3738, 0xcc);
	write_cmos_sensor(0x373d, 0x26);
	write_cmos_sensor(0x3764, 0x20);
	write_cmos_sensor(0x3765, 0x20);
	write_cmos_sensor(0x37a1, 0x36);
	write_cmos_sensor(0x37a8, 0x3b);
	write_cmos_sensor(0x37ab, 0x31);
	write_cmos_sensor(0x37c3, 0xf1);
	write_cmos_sensor(0x37c5, 0x00);
	write_cmos_sensor(0x37d8, 0x03);
	write_cmos_sensor(0x37da, 0xc2);
	write_cmos_sensor(0x37dc, 0x02);
	write_cmos_sensor(0x37e0, 0x00);
	write_cmos_sensor(0x37e1, 0x0a);
	write_cmos_sensor(0x37e2, 0x14);
	write_cmos_sensor(0x37e5, 0x03);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3821, 0x00);
	write_cmos_sensor(0x3822, 0xc2);
	write_cmos_sensor(0x3823, 0x18);
	write_cmos_sensor(0x3832, 0x00);
	write_cmos_sensor(0x3c80, 0x00);
	write_cmos_sensor(0x3c87, 0x01);
	write_cmos_sensor(0x3c8c, 0x19);
	write_cmos_sensor(0x3c8d, 0x1c);
	write_cmos_sensor(0x3c90, 0x00);
	write_cmos_sensor(0x3c91, 0x00);
	write_cmos_sensor(0x3c92, 0x00);
	write_cmos_sensor(0x3c93, 0x00);
	write_cmos_sensor(0x3c94, 0x41);
	write_cmos_sensor(0x3c95, 0x54);
	write_cmos_sensor(0x3c96, 0x34);
	write_cmos_sensor(0x3c97, 0x04);
	write_cmos_sensor(0x3c98, 0x00);
	write_cmos_sensor(0x3d8c, 0x73);
	write_cmos_sensor(0x3d8d, 0xc0);
	write_cmos_sensor(0x3f00, 0x0b);
	write_cmos_sensor(0x3f03, 0x00);
	write_cmos_sensor(0x4001, 0xe0);
	write_cmos_sensor(0x4008, 0x00);
	write_cmos_sensor(0x4011, 0xf0);
	write_cmos_sensor(0x4052, 0x00);
	write_cmos_sensor(0x4053, 0x86);
	write_cmos_sensor(0x4054, 0x00);
	write_cmos_sensor(0x4055, 0x86);
	write_cmos_sensor(0x4056, 0x00);
	write_cmos_sensor(0x4057, 0x86);
	write_cmos_sensor(0x4058, 0x00);
	write_cmos_sensor(0x4059, 0x86);
	write_cmos_sensor(0x405e, 0x20);
	write_cmos_sensor(0x4500, 0x07);
	write_cmos_sensor(0x4503, 0x00);
	write_cmos_sensor(0x450a, 0x04);
	write_cmos_sensor(0x4809, 0x04);
	write_cmos_sensor(0x480c, 0x12);
	write_cmos_sensor(0x4833, 0x10);
	write_cmos_sensor(0x4d00, 0x03);
	write_cmos_sensor(0x4d01, 0xc9);
	write_cmos_sensor(0x4d02, 0xbc);
	write_cmos_sensor(0x4d03, 0xd7);
	write_cmos_sensor(0x4d04, 0xf0);
	write_cmos_sensor(0x4d05, 0xa2);
	write_cmos_sensor(0x5000, 0xff);
	write_cmos_sensor(0x5001, 0x07);
	write_cmos_sensor(0x5040, 0x39);
	write_cmos_sensor(0x5041, 0x10);
	write_cmos_sensor(0x5042, 0x10);
	write_cmos_sensor(0x5043, 0x84);
	write_cmos_sensor(0x5044, 0x62);
	write_cmos_sensor(0x5180, 0x00);
	write_cmos_sensor(0x5181, 0x10);
	write_cmos_sensor(0x5182, 0x00);
	write_cmos_sensor(0x5183, 0x67);
	write_cmos_sensor(0x5200, 0x1b);
	write_cmos_sensor(0x520b, 0x07);
	write_cmos_sensor(0x520c, 0x0f);
	write_cmos_sensor(0x5300, 0x04);
	write_cmos_sensor(0x5301, 0x0c);
	write_cmos_sensor(0x5302, 0x0c);
	write_cmos_sensor(0x5303, 0x0f);
	write_cmos_sensor(0x5304, 0x00);
	write_cmos_sensor(0x5305, 0x70);
	write_cmos_sensor(0x5306, 0x00);
	write_cmos_sensor(0x5307, 0x80);
	write_cmos_sensor(0x5308, 0x00);
	write_cmos_sensor(0x5309, 0xa5);
	write_cmos_sensor(0x530a, 0x00);
	write_cmos_sensor(0x530b, 0xd3);
	write_cmos_sensor(0x530c, 0x00);
	write_cmos_sensor(0x530d, 0xf0);
	write_cmos_sensor(0x530e, 0x01);
	write_cmos_sensor(0x530f, 0x10);
	write_cmos_sensor(0x5310, 0x01);
	write_cmos_sensor(0x5311, 0x20);
	write_cmos_sensor(0x5312, 0x01);
	write_cmos_sensor(0x5313, 0x20);
	write_cmos_sensor(0x5314, 0x01);
	write_cmos_sensor(0x5315, 0x20);
	write_cmos_sensor(0x5316, 0x08);
	write_cmos_sensor(0x5317, 0x08);
	write_cmos_sensor(0x5318, 0x10);
	write_cmos_sensor(0x5319, 0x88);
	write_cmos_sensor(0x531a, 0x88);
	write_cmos_sensor(0x531b, 0xa9);
	write_cmos_sensor(0x531c, 0xaa);
	write_cmos_sensor(0x531d, 0x0a);
	write_cmos_sensor(0x5405, 0x02);
	write_cmos_sensor(0x5406, 0x67);
	write_cmos_sensor(0x5407, 0x01);
	write_cmos_sensor(0x5408, 0x4a);

	write_cmos_sensor(0x0100, 0x00);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_preview_ov13b10[] = {
	0x0305, 0x23,
	0x3501, 0x06,
	0x3502, 0x10,
	0x3662, 0x88,
	0x3714, 0x28,
	0x371a, 0x3e,
	0x3739, 0x10,
	0x37c2, 0x14,
	0x37d9, 0x06,
	0x3800, 0x00,
	0x3801, 0x00,
	0x3802, 0x00,
	0x3803, 0x08,
	0x3804, 0x10,
	0x3805, 0x8f,
	0x3806, 0x0c,
	0x3807, 0x47,
	0x3808, 0x08,
	0x3809, 0x38,
	0x380a, 0x06,
	0x380b, 0x18,
	0x380e, 0x0c,
	0x380f, 0x7c,
	0x3811, 0x07,
	0x3813, 0x04,
	0x3814, 0x03,
	0x3816, 0x03,
	0x3820, 0x8b,
	0x4008, 0x00,
	0x4009, 0x05,
	0x4050, 0x00,
	0x4051, 0x05,
	0x4501, 0x08,
	0x4505, 0x04,
	0x4800, 0x64,
	0x4837, 0x1d,
	0x5000, 0xfd,
	0x5001, 0x0d
};
#endif

static void preview_setting(void)
{
	cam_pr_debug("E RES_2112x1568_30fps\n");
#if MULTI_WRITE
	ov13b10_table_write_cmos_sensor(
		addr_data_pair_preview_ov13b10,
		sizeof(addr_data_pair_preview_ov13b10) / sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x3501, 0x40);
	write_cmos_sensor(0x3662, 0x10);
	write_cmos_sensor(0x3714, 0x28);
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x14);
	write_cmos_sensor(0x37d9, 0x0c);
	write_cmos_sensor(0x37e3, 0x08);
	write_cmos_sensor(0x37e4, 0x34);
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x57);
	write_cmos_sensor(0x3808, 0x08);
	write_cmos_sensor(0x3809, 0x40);
	write_cmos_sensor(0x380a, 0x06);
	write_cmos_sensor(0x380b, 0x20);
	write_cmos_sensor(0x380c, 0x08);
	write_cmos_sensor(0x380d, 0xc4);
	write_cmos_sensor(0x380e, 0x06);
	write_cmos_sensor(0x380f, 0x48);
	write_cmos_sensor(0x3811, 0x08);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3816, 0x03);
	write_cmos_sensor(0x3820, 0xab);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x90);
	write_cmos_sensor(0x3829, 0x07);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x1c);
	write_cmos_sensor(0x4902, 0x01);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_15fps_ov13b10[] = {
	0x0305, 0x46,
	0x3501, 0x0c,
	0x3502, 0x10,
	0x3662, 0x92,
	0x3714, 0x24,
	0x371a, 0x3e,
	0x3739, 0x12,
	0x37c2, 0x04,
	0x37d9, 0x0c,
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
	0x380e, 0x18,
	0x380f, 0xf8,
	0x3811, 0x0f,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3816, 0x01,
	0x3820, 0x88,
	0x4008, 0x02,
	0x4009, 0x0f,
	0x4050, 0x02,
	0x4051, 0x09,
	0x4501, 0x00,
	0x4505, 0x00,
	0x4800, 0x64,
	0x4837, 0x0e,
	0x5000, 0xff,
	0x5001, 0x0f
};

kal_uint16 addr_data_pair_capture_30fps_ov13b10[] = {
	0x0305, 0x46,
	0x3501, 0x0c,
	0x3502, 0x10,
	0x3662, 0x92,
	0x3714, 0x24,
	0x371a, 0x3e,
	0x3739, 0x12,
	0x37c2, 0x04,
	0x37d9, 0x0c,
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
	0x380e, 0x0c,
	0x380f, 0x7c,
	0x3811, 0x0f,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3816, 0x01,
	0x3820, 0x88,
	0x4008, 0x02,
	0x4009, 0x0f,
	0x4050, 0x02,
	0x4051, 0x09,
	0x4501, 0x00,
	0x4505, 0x00,
	0x4800, 0x64,
	0x4837, 0x0e,
	0x5000, 0xff,
	0x5001, 0x0f
};
#endif

static void capture_setting(kal_uint16 currefps)
{

	cam_pr_debug("E 4224x3136_zsl_30fps currefps = %d\n",
		currefps);
#if MULTI_WRITE
	if (currefps == 150) {
		ov13b10_table_write_cmos_sensor(
			addr_data_pair_capture_15fps_ov13b10,
			sizeof(addr_data_pair_capture_15fps_ov13b10) /
			sizeof(kal_uint16));
	} else {
		ov13b10_table_write_cmos_sensor(
			addr_data_pair_capture_30fps_ov13b10,
			sizeof(addr_data_pair_capture_30fps_ov13b10) /
			sizeof(kal_uint16));
	}
#else
	if (currefps == 150) {
		write_cmos_sensor(0x0303, 0x00);
		write_cmos_sensor(0x3501, 0x80);
		write_cmos_sensor(0x3662, 0x12);
		write_cmos_sensor(0x3714, 0x24);
		write_cmos_sensor(0x3737, 0x04);
		write_cmos_sensor(0x3739, 0x12);
		write_cmos_sensor(0x37c2, 0x04);
		write_cmos_sensor(0x37d9, 0x0c);
		write_cmos_sensor(0x37e3, 0x04);
		write_cmos_sensor(0x37e4, 0x26);
		write_cmos_sensor(0x37e6, 0x04);
		write_cmos_sensor(0x3801, 0x00);
		write_cmos_sensor(0x3802, 0x00);
		write_cmos_sensor(0x3803, 0x08);
		write_cmos_sensor(0x3805, 0x9f);
		write_cmos_sensor(0x3806, 0x0c);
		write_cmos_sensor(0x3807, 0x57);
		write_cmos_sensor(0x3808, 0x10);
		write_cmos_sensor(0x3809, 0x80);
		write_cmos_sensor(0x380a, 0x0c);
		write_cmos_sensor(0x380b, 0x40);
		write_cmos_sensor(0x380c, 0x04);
		write_cmos_sensor(0x380d, 0x62);

		write_cmos_sensor(0x380e, 0x0f);
		write_cmos_sensor(0x380f, 0xaa);

		write_cmos_sensor(0x3811, 0x10);
		write_cmos_sensor(0x3813, 0x08);
		write_cmos_sensor(0x3814, 0x01);
		write_cmos_sensor(0x3816, 0x01);
		write_cmos_sensor(0x3820, 0xa8);
		write_cmos_sensor(0x3826, 0x11);
		write_cmos_sensor(0x3827, 0x1c);
		write_cmos_sensor(0x3829, 0x03);
		write_cmos_sensor(0x4009, 0x0f);
		write_cmos_sensor(0x4050, 0x04);
		write_cmos_sensor(0x4051, 0x0b);
		write_cmos_sensor(0x4837, 0x0e);
		write_cmos_sensor(0x4902, 0x01);
	} else {
		write_cmos_sensor(0x0303, 0x00);
		write_cmos_sensor(0x3501, 0x80);
		write_cmos_sensor(0x3662, 0x12);
		write_cmos_sensor(0x3714, 0x24);
		write_cmos_sensor(0x3737, 0x04);
		write_cmos_sensor(0x3739, 0x12);
		write_cmos_sensor(0x37c2, 0x04);
		write_cmos_sensor(0x37d9, 0x0c);
		write_cmos_sensor(0x37e3, 0x04);
		write_cmos_sensor(0x37e4, 0x26);
		write_cmos_sensor(0x37e6, 0x04);
		write_cmos_sensor(0x3801, 0x00);
		write_cmos_sensor(0x3802, 0x00);
		write_cmos_sensor(0x3803, 0x08);
		write_cmos_sensor(0x3805, 0x9f);
		write_cmos_sensor(0x3806, 0x0c);
		write_cmos_sensor(0x3807, 0x57);
		write_cmos_sensor(0x3808, 0x10);
		write_cmos_sensor(0x3809, 0x80);
		write_cmos_sensor(0x380a, 0x0c);
		write_cmos_sensor(0x380b, 0x40);
		write_cmos_sensor(0x380c, 0x04);
		write_cmos_sensor(0x380d, 0x62);
		write_cmos_sensor(0x380e, 0x0C);
		write_cmos_sensor(0x380f, 0x8E);
		write_cmos_sensor(0x3811, 0x10);
		write_cmos_sensor(0x3813, 0x08);
		write_cmos_sensor(0x3814, 0x01);
		write_cmos_sensor(0x3816, 0x01);
		write_cmos_sensor(0x3820, 0xa8);
		write_cmos_sensor(0x3826, 0x11);
		write_cmos_sensor(0x3827, 0x1c);
		write_cmos_sensor(0x3829, 0x03);
		write_cmos_sensor(0x4009, 0x0f);
		write_cmos_sensor(0x4050, 0x04);
		write_cmos_sensor(0x4051, 0x0b);
		write_cmos_sensor(0x4837, 0x0e);
		write_cmos_sensor(0x4902, 0x01);
	}
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_video_ov13b10[] = {
	0x0303, 0x00,
	0x3501, 0x80,
	0x3662, 0x12,
	0x3714, 0x24,
	0x3737, 0x04,
	0x3739, 0x12,
	0x37c2, 0x04,
	0x37d9, 0x0c,
	0x37e3, 0x04,
	0x37e4, 0x26,
	0x37e6, 0x04,
	0x3801, 0x00,
	0x3802, 0x00,
	0x3803, 0x08,
	0x3805, 0x9f,
	0x3806, 0x0c,
	0x3807, 0x57,
	0x3808, 0x10,
	0x3809, 0x80,
	0x380a, 0x0c,
	0x380b, 0x40,
	0x380c, 0x04,
	0x380d, 0x62,
	0x380e, 0x0C,
	0x380f, 0x8E,
	0x3811, 0x10,
	0x3813, 0x08,
	0x3814, 0x01,
	0x3816, 0x01,
	0x3820, 0xa8,
	0x3826, 0x11,
	0x3827, 0x1c,
	0x3829, 0x03,
	0x4009, 0x0f,
	0x4050, 0x04,
	0x4051, 0x0b,
	0x4837, 0x0e,
	0x4902, 0x01
};
#endif

#undef VIDEO_REPLACE_BY_PREVIEW
#ifdef VIDEO_REPLACE_BY_PREVIEW
static void normal_video_setting(kal_uint16 currefps)
{
	cam_pr_debug("E RES_4224x3136_zsl_30fps\n");
#if MULTI_WRITE
	ov13b10_table_write_cmos_sensor(
		addr_data_pair_video_ov13b10,
		sizeof(addr_data_pair_video_ov13b10) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x3501, 0x80);
	write_cmos_sensor(0x3662, 0x12);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x3737, 0x04);
	write_cmos_sensor(0x3739, 0x12);
	write_cmos_sensor(0x37c2, 0x04);
	write_cmos_sensor(0x37d9, 0x0c);
	write_cmos_sensor(0x37e3, 0x04);
	write_cmos_sensor(0x37e4, 0x26);
	write_cmos_sensor(0x37e6, 0x04);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x57);
	write_cmos_sensor(0x3808, 0x10);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x0c);
	write_cmos_sensor(0x380b, 0x40);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0C);
	write_cmos_sensor(0x380f, 0x8E);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3813, 0x08);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3820, 0xa8);
	write_cmos_sensor(0x3826, 0x11);
	write_cmos_sensor(0x3827, 0x1c);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x0f);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x0e);
	write_cmos_sensor(0x4902, 0x01);
#endif
}
#endif

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_ov13b10[] = {
	0x0303, 0x03,
	0x3501, 0x20,
	0x3662, 0x08,
	0x3714, 0x30,
	0x3737, 0x08,
	0x3739, 0x20,
	0x37c2, 0x2c,
	0x37d9, 0x06,
	0x37e3, 0x08,
	0x37e4, 0x34,
	0x37e6, 0x08,
	0x3801, 0x40,
	0x3802, 0x00,
	0x3803, 0x40,
	0x3805, 0x5f,
	0x3806, 0x0c,
	0x3807, 0x5f,
	0x3808, 0x04,
	0x3809, 0x00,
	0x380a, 0x03,
	0x380b, 0x00,
	0x380c, 0x04,
	0x380d, 0x62,
	0x380e, 0x03,
	0x380f, 0x24,
	0x3808, 0x02,
	0x3809, 0x80,
	0x380a, 0x01,
	0x380b, 0xE0,
	0x3811, 0xc4,
	0x3813, 0x94,
	0x3814, 0x07,
	0x3816, 0x07,
	0x3820, 0xac,
	0x3826, 0x04,
	0x3827, 0x48,
	0x3829, 0x03,
	0x4009, 0x05,
	0x4050, 0x02,
	0x4051, 0x05,
	0x4837, 0x38,
	0x4902, 0x02
};
#endif

static void hs_video_setting(void)
{
	cam_pr_debug("E RES_640x480_120fps\n");
#if MULTI_WRITE
	ov13b10_table_write_cmos_sensor(
		addr_data_pair_hs_video_ov13b10,
		sizeof(addr_data_pair_hs_video_ov13b10) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0303, 0x03);//PLL
	write_cmos_sensor(0x3501, 0x20);
	write_cmos_sensor(0x3662, 0x08);
	write_cmos_sensor(0x3714, 0x30);
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x2c);
	write_cmos_sensor(0x37d9, 0x06);
	write_cmos_sensor(0x37e3, 0x08);
	write_cmos_sensor(0x37e4, 0x34);
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3801, 0x40);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x40);
	write_cmos_sensor(0x3805, 0x5f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x5f);
	write_cmos_sensor(0x3808, 0x04);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x03);
	write_cmos_sensor(0x380b, 0x00);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);

#if 0 //1024x768
	write_cmos_sensor(0x380e, 0x03);
	write_cmos_sensor(0x380f, 0x2a);

	write_cmos_sensor(0x380a, 0x03);
	write_cmos_sensor(0x380b, 0x00);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x3811, 0x04);
	write_cmos_sensor(0x3813, 0x04);
#else
	write_cmos_sensor(0x380e, 0x03);
	write_cmos_sensor(0x380f, 0x24);
	write_cmos_sensor(0x3808, 0x02);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x01);
	write_cmos_sensor(0x380b, 0xE0);
	write_cmos_sensor(0x3811, 0xc4);
	write_cmos_sensor(0x3813, 0x94);
#endif

	write_cmos_sensor(0x3814, 0x07);
	write_cmos_sensor(0x3816, 0x07);
	write_cmos_sensor(0x3820, 0xac);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x48);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x05);
	write_cmos_sensor(0x4050, 0x02);
	write_cmos_sensor(0x4051, 0x05);
	write_cmos_sensor(0x4837, 0x38);
	write_cmos_sensor(0x4902, 0x02);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_ov13b10[] = {
	0x0303, 0x03,
	0x3501, 0x20,
	0x3662, 0x08,
	0x3714, 0x30,
	0x3737, 0x08,
	0x3739, 0x20,
	0x37c2, 0x2c,
	0x37d9, 0x06,
	0x37e3, 0x08,
	0x37e4, 0x34,
	0x37e6, 0x08,
	0x3801, 0x40,
	0x3802, 0x00,
	0x3803, 0x40,
	0x3805, 0x5f,
	0x3806, 0x0c,
	0x3807, 0x5f,
	0x3808, 0x04,
	0x3809, 0x00,
	0x380a, 0x03,
	0x380b, 0x00,
	0x380c, 0x04,
	0x380d, 0x62,
	0x380e, 0x0c,
	0x380f, 0x90,
	0x3811, 0x04,
	0x3813, 0x04,
	0x3814, 0x07,
	0x3816, 0x07,
	0x3820, 0xac,
	0x3826, 0x04,
	0x3827, 0x48,
	0x3829, 0x03,
	0x4009, 0x05,
	0x4050, 0x02,
	0x4051, 0x05,
	0x4837, 0x38,
	0x4902, 0x02
};
#endif

static void slim_video_setting(void)
{
	cam_pr_debug("E\n");
#if MULTI_WRITE
	ov13b10_table_write_cmos_sensor(
		addr_data_pair_slim_video_ov13b10,
		sizeof(addr_data_pair_slim_video_ov13b10) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0303, 0x03);//PLL
	write_cmos_sensor(0x3501, 0x20);
	write_cmos_sensor(0x3662, 0x08);
	write_cmos_sensor(0x3714, 0x30);
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x2c);
	write_cmos_sensor(0x37d9, 0x06);
	write_cmos_sensor(0x37e3, 0x08);
	write_cmos_sensor(0x37e4, 0x34);
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3801, 0x40);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x40);
	write_cmos_sensor(0x3805, 0x5f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x5f);
	write_cmos_sensor(0x3808, 0x04);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x03);
	write_cmos_sensor(0x380b, 0x00);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x90);
	write_cmos_sensor(0x3811, 0x04);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x07);
	write_cmos_sensor(0x3816, 0x07);
	write_cmos_sensor(0x3820, 0xac);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x48);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x05);
	write_cmos_sensor(0x4050, 0x02);
	write_cmos_sensor(0x4051, 0x05);
	write_cmos_sensor(0x4837, 0x38);
	write_cmos_sensor(0x4902, 0x02);
#endif
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x300a) << 16) |
		(read_cmos_sensor(0x300b) << 8) | read_cmos_sensor(0x300c));
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
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			cam_pr_debug("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 1;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 1;
	kal_uint32 sensor_id = 0;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
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
		cam_pr_debug("Open sensor id: 0x%x fail\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}

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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.pdaf_mode = 0;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	return ERROR_NONE;
}   /*  close  */

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor.current_fps;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}

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
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			cam_pr_debug("current_fps %d fps is not support,use cap1: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	}

	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;
}   /* capture() */

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
	spin_unlock(&imgsensor_drv_lock);
	//normal_video_setting(imgsensor.current_fps);
	preview_setting();
	return ERROR_NONE;
}

static kal_uint32 hs_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}

static kal_uint32 slim_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}

static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
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

	return ERROR_NONE;
}   /*  get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
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
	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode*/
	sensor_info->PDAF_Support = 1;

	//sensor_info->HDR_Support = 0; /*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
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
}   /*  get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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
		cam_pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
	return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}   /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	if (framerate == 0)
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

static kal_uint32 set_auto_flicker_mode(kal_bool enable,
			UINT16 framerate)
{
	cam_pr_debug("enable = %d, framerate = %d\n",
		enable, framerate);

	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frameHeight;

	cam_pr_debug("scenario_id = %d, framerate = %d\n",
			scenario_id, framerate);

	if (framerate == 0)
		return ERROR_NONE;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength) : 0;
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
		frameHeight = imgsensor_info.normal_video.pclk /
			framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.normal_video.framelength) ?
			(frameHeight -
			 imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frameHeight = imgsensor_info.cap.pclk / framerate * 10 /
			imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.cap.framelength) ?
			(frameHeight - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frameHeight = imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.hs_video.framelength) ?
			(frameHeight - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frameHeight = imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.slim_video.framelength) ?
			(frameHeight -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 *framerate)
{
	cam_pr_debug("[3058]scenario_id = %d\n", scenario_id);

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
	if (enable) {
		write_cmos_sensor(0x5000, 0x81);
		write_cmos_sensor(0x5080, 0x80);
	} else {
		write_cmos_sensor(0x5000, 0xff);
		write_cmos_sensor(0x5080, 0x00);
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	UINT32 temperature = 0;
	INT32 temperature_convert = 0;

	/*TEMP_SEN_CTL */
	write_cmos_sensor(0x4d12, 0x01);
	temperature = (read_cmos_sensor(0x4d13) << 8) |
		read_cmos_sensor(0x4d13);
	if (temperature < 0xc000)
		temperature_convert = temperature / 256;
	else
		temperature_convert = 192 - temperature / 256;

	if (temperature_convert > 192)
		temperature_convert = 192;
	else if (temperature_convert < -64)
		temperature_convert = -64;

	return 20;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	cam_pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
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
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;

	UINT32 rate = 0;

	if (!((feature_id == 3040) || (feature_id == 3058)))
		cam_pr_debug("feature_id = %d\n", feature_id);

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
		night_mode((BOOL) * feature_data);
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
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		cam_pr_debug("current fps :%d\n", imgsensor.current_fps);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		cam_pr_debug("GET_CROP_INFO scenarioId:%d\n",
			*feature_data_32);

		wininfo = (struct  SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
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
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
			(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			break;
		}
		break;

	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
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
	case SENSOR_FEATURE_SET_PDAF:
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;

	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			rate = imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			rate = imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			rate = imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			rate = imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}   /*  feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV13B10_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}
