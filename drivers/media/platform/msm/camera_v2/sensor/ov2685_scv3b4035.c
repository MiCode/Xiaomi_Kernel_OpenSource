/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm.h"

#define OV2685_SCV3B4035_SENSOR_NAME "ov2685_scv3b4035"
#define PLATFORM_DRIVER_NAME "msm_camera_ov2685_scv3b4035"
#define ov2685_scv3b4035_obj ov2685_scv3b4035_##obj
#define CCI_I2C_MAX_WRITE 8192

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

DEFINE_MSM_MUTEX(ov2685_scv3b4035_mut);
static struct msm_sensor_ctrl_t ov2685_scv3b4035_s_ctrl;

static struct msm_sensor_power_setting ov2685_scv3b4035_power_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},

};

static struct msm_camera_i2c_reg_conf ov2685_scv3b4035_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf ov2685_scv3b4035_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf
	ov2685_scv3b4035_1600x1200p30_settings[] = {
	{0x0103, 0x01},
	{0x3002, 0x00},
	{0x3016, 0x1C},
	{0x3018, 0x84},
	{0x301D, 0xF0},
	{0x3020, 0x00},
	{0x3082, 0x2C},
	{0x3083, 0x03},
	{0x3084, 0x07},
	{0x3085, 0x03},
	{0x3086, 0x00},
	{0x3087, 0x00},
	{0x3501, 0x4E},
	{0x3502, 0xE0},
	{0x3503, 0x03},
	{0x350B, 0x36},
	{0x3600, 0xB4},
	{0x3603, 0x35},
	{0x3604, 0x24},
	{0x3605, 0x00},
	{0x3620, 0x24},
	{0x3621, 0x34},
	{0x3622, 0x03},
	{0x3628, 0x10},
	{0x3705, 0x3C},
	{0x370A, 0x21},
	{0x370C, 0x50},
	{0x370D, 0xC0},
	{0x3717, 0x58},
	{0x3718, 0x80},
	{0x3720, 0x00},
	{0x3721, 0x09},
	{0x3722, 0x06},
	{0x3723, 0x59},
	{0x3738, 0x99},
	{0x3781, 0x80},
	{0x3784, 0x0C},
	{0x3789, 0x60},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x06},
	{0x3805, 0x4F},
	{0x3806, 0x04},
	{0x3807, 0xBF},
	{0x3808, 0x06},
	{0x3809, 0x40},
	{0x380A, 0x04},
	{0x380B, 0xB0},
	{0x380C, 0x06},
	{0x380D, 0xA4},
	{0x380E, 0x05},
	{0x380F, 0x0E},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x08},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3819, 0x04},
	{0x3820, 0xC0},
	{0x3821, 0x00},
	{0x3A06, 0x01},
	{0x3A07, 0x84},
	{0x3A08, 0x01},
	{0x3A09, 0x43},
	{0x3A0A, 0x24},
	{0x3A0B, 0x60},
	{0x3A0C, 0x28},
	{0x3A0D, 0x60},
	{0x3A0E, 0x04},
	{0x3A0F, 0x8C},
	{0x3A10, 0x05},
	{0x3A11, 0x0C},
	{0x4000, 0x81},
	{0x4001, 0x40},
	{0x4008, 0x02},
	{0x4009, 0x09},
	{0x4300, 0x30},
	{0x430E, 0x00},
	{0x4602, 0x02},
	{0x481B, 0x40},
	{0x481F, 0x40},
	{0x4837, 0x1E},
	{0x5000, 0xFF},
	{0x5001, 0x05},
	{0x5002, 0x32},
	{0x5003, 0x04},
	{0x5004, 0xFF},
	{0x5005, 0x12},
	{0x0100, 0x00},
	{0x5180, 0xF4},
	{0x5181, 0x11},
	{0x5182, 0x41},
	{0x5183, 0x42},
	{0x5184, 0x78},
	{0x5185, 0x58},
	{0x5186, 0xB5},
	{0x5187, 0xB2},
	{0x5188, 0x08},
	{0x5189, 0x0E},
	{0x518A, 0x0C},
	{0x518B, 0x4C},
	{0x518C, 0x38},
	{0x518D, 0xF8},
	{0x518E, 0x04},
	{0x518F, 0x7F},
	{0x5190, 0x40},
	{0x5191, 0x5F},
	{0x5192, 0x40},
	{0x5193, 0xFF},
	{0x5194, 0x40},
	{0x5195, 0x07},
	{0x5196, 0x04},
	{0x5197, 0x04},
	{0x5198, 0x00},
	{0x5199, 0x05},
	{0x519A, 0xD2},
	{0x519B, 0x04},
	{0x5200, 0x09},
	{0x5201, 0x00},
	{0x5202, 0x06},
	{0x5203, 0x20},
	{0x5204, 0x41},
	{0x5205, 0x16},
	{0x5206, 0x00},
	{0x5207, 0x05},
	{0x520B, 0x30},
	{0x520C, 0x75},
	{0x520D, 0x00},
	{0x520E, 0x30},
	{0x520F, 0x75},
	{0x5210, 0x00},
	{0x5280, 0x14},
	{0x5281, 0x02},
	{0x5282, 0x02},
	{0x5283, 0x04},
	{0x5284, 0x06},
	{0x5285, 0x08},
	{0x5286, 0x0C},
	{0x5287, 0x10},
	{0x5300, 0xC5},
	{0x5301, 0xA0},
	{0x5302, 0x06},
	{0x5303, 0x0A},
	{0x5304, 0x30},
	{0x5305, 0x60},
	{0x5306, 0x90},
	{0x5307, 0xC0},
	{0x5308, 0x82},
	{0x5309, 0x00},
	{0x530A, 0x26},
	{0x530B, 0x02},
	{0x530C, 0x02},
	{0x530D, 0x00},
	{0x530E, 0x0C},
	{0x530F, 0x14},
	{0x5310, 0x1A},
	{0x5311, 0x20},
	{0x5312, 0x80},
	{0x5313, 0x4B},
	{0x5380, 0x01},
	{0x5381, 0x52},
	{0x5382, 0x00},
	{0x5383, 0x4A},
	{0x5384, 0x00},
	{0x5385, 0xB6},
	{0x5386, 0x00},
	{0x5387, 0x8D},
	{0x5388, 0x00},
	{0x5389, 0x3A},
	{0x538A, 0x00},
	{0x538B, 0xA6},
	{0x538C, 0x00},
	{0x5400, 0x0D},
	{0x5401, 0x18},
	{0x5402, 0x31},
	{0x5403, 0x5A},
	{0x5404, 0x65},
	{0x5405, 0x6F},
	{0x5406, 0x77},
	{0x5407, 0x80},
	{0x5408, 0x87},
	{0x5409, 0x8F},
	{0x540A, 0xA2},
	{0x540B, 0xB2},
	{0x540C, 0xCC},
	{0x540D, 0xE4},
	{0x540E, 0xF0},
	{0x540F, 0xA0},
	{0x5410, 0x6E},
	{0x5411, 0x06},
	{0x5480, 0x19},
	{0x5481, 0x00},
	{0x5482, 0x09},
	{0x5483, 0x12},
	{0x5484, 0x04},
	{0x5485, 0x06},
	{0x5486, 0x08},
	{0x5487, 0x0C},
	{0x5488, 0x10},
	{0x5489, 0x18},
	{0x5500, 0x02},
	{0x5501, 0x03},
	{0x5502, 0x04},
	{0x5503, 0x05},
	{0x5504, 0x06},
	{0x5505, 0x08},
	{0x5506, 0x00},
	{0x5600, 0x02},
	{0x5603, 0x40},
	{0x5604, 0x28},
	{0x5609, 0x20},
	{0x560A, 0x60},
	{0x5780, 0x3E},
	{0x5781, 0x0F},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x01},
	{0x5786, 0x00},
	{0x5787, 0x04},
	{0x5788, 0x02},
	{0x5789, 0x00},
	{0x578A, 0x01},
	{0x578B, 0x02},
	{0x578C, 0x03},
	{0x578D, 0x03},
	{0x578E, 0x08},
	{0x578F, 0x0C},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x00},
	{0x5794, 0x03},
	{0x5800, 0x03},
	{0x5801, 0x24},
	{0x5802, 0x02},
	{0x5803, 0x40},
	{0x5804, 0x34},
	{0x5805, 0x05},
	{0x5806, 0x12},
	{0x5807, 0x05},
	{0x5808, 0x03},
	{0x5809, 0x3C},
	{0x580A, 0x02},
	{0x580B, 0x40},
	{0x580C, 0x26},
	{0x580D, 0x05},
	{0x580E, 0x52},
	{0x580F, 0x06},
	{0x5810, 0x03},
	{0x5811, 0x28},
	{0x5812, 0x02},
	{0x5813, 0x40},
	{0x5814, 0x24},
	{0x5815, 0x05},
	{0x5816, 0x42},
	{0x5817, 0x06},
	{0x5818, 0x0D},
	{0x5819, 0x40},
	{0x581A, 0x04},
	{0x581B, 0x0C},
	{0x3A03, 0x4C},
	{0x3A04, 0x40},
	{0x3080, 0x02}, //change for 24fps
	{0x3082, 0x48}, //change for 24fps
	{0x3018, 0x44}, //change for 24fps
	{0x3084, 0x0F}, //change for 24fps
	{0x3085, 0x06}, //change for 24fps
	{0x380d, 0xc8}, //change for 24fps
	{0x380f, 0x10}, //change for 24fps
	{0x4837, 0x12}, //change for 24fps

	/* FSIN setup */
	{0x3002, 0x00},
	{0x3823, 0x30},
	{0x3824, 0x00},
	{0x3825, 0x10},
	{0x3826, 0x00},
	{0x3827, 0x08},
};

static struct msm_camera_i2c_reg_conf ov2685_scv3b4035_720p60_settings[] = {
	{0x0103, 0x01},
	{0x3002, 0x00},
	{0x3016, 0x1C},
	{0x3018, 0x84},
	{0x301D, 0xF0},
	{0x3020, 0x00},
	{0x3082, 0x2C},
	{0x3083, 0x03},
	{0x3084, 0x07},
	{0x3085, 0x03},
	{0x3086, 0x00},
	{0x3087, 0x00},
	{0x3501, 0x2D},
	{0x3502, 0x80},
	{0x3503, 0x03},
	{0x350B, 0x36},
	{0x3600, 0xB4},
	{0x3603, 0x35},
	{0x3604, 0x24},
	{0x3605, 0x00},
	{0x3620, 0x26},
	{0x3621, 0x37},
	{0x3622, 0x04},
	{0x3628, 0x10},
	{0x3705, 0x3C},
	{0x370A, 0x21},
	{0x370C, 0x50},
	{0x370D, 0xC0},
	{0x3717, 0x58},
	{0x3718, 0x88},
	{0x3720, 0x00},
	{0x3721, 0x00},
	{0x3722, 0x00},
	{0x3723, 0x00},
	{0x3738, 0x00},
	{0x3781, 0x80},
	{0x3784, 0x0C},
	{0x3789, 0x60},
	{0x3800, 0x00},
	{0x3801, 0xA0},
	{0x3802, 0x00},
	{0x3803, 0xF2},
	{0x3804, 0x05},
	{0x3805, 0xAF},
	{0x3806, 0x03},
	{0x3807, 0xCD},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380A, 0x02},
	{0x380B, 0xD0},
	{0x380C, 0x05},
	{0x380D, 0xA6},
	{0x380E, 0x02},
	{0x380F, 0xF8},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3819, 0x04},
	{0x3820, 0xC0},
	{0x3821, 0x00},
	{0x3A06, 0x01},
	{0x3A07, 0xC8},
	{0x3A08, 0x01},
	{0x3A09, 0x7C},
	{0x3A0A, 0x0E},
	{0x3A0B, 0x40},
	{0x3A0C, 0x17},
	{0x3A0D, 0xC0},
	{0x3A0E, 0x01},
	{0x3A0F, 0xC8},
	{0x3A10, 0x02},
	{0x3A11, 0xF8},
	{0x4000, 0x81},
	{0x4001, 0x40},
	{0x4008, 0x02},
	{0x4009, 0x09},
	{0x4300, 0x30},
	{0x430E, 0x00},
	{0x4602, 0x02},
	{0x481B, 0x40},
	{0x481F, 0x40},
	{0x4837, 0x1E},
	{0x5000, 0xFF},
	{0x5001, 0x05},
	{0x5002, 0x32},
	{0x5003, 0x04},
	{0x5004, 0xFF},
	{0x5005, 0x12},
	{0x5180, 0xF4},
	{0x5181, 0x11},
	{0x5182, 0x41},
	{0x5183, 0x42},
	{0x5184, 0x78},
	{0x5185, 0x58},
	{0x5186, 0xB5},
	{0x5187, 0xB2},
	{0x5188, 0x08},
	{0x5189, 0x0E},
	{0x518A, 0x0C},
	{0x518B, 0x4C},
	{0x518C, 0x38},
	{0x518D, 0xF8},
	{0x518E, 0x04},
	{0x518F, 0x7F},
	{0x5190, 0x40},
	{0x5191, 0x5F},
	{0x5192, 0x40},
	{0x5193, 0xFF},
	{0x5194, 0x40},
	{0x5195, 0x07},
	{0x5196, 0x04},
	{0x5197, 0x04},
	{0x5198, 0x00},
	{0x5199, 0x05},
	{0x519A, 0xD2},
	{0x519B, 0x04},
	{0x5200, 0x09},
	{0x5201, 0x00},
	{0x5202, 0x06},
	{0x5203, 0x20},
	{0x5204, 0x41},
	{0x5205, 0x16},
	{0x5206, 0x00},
	{0x5207, 0x05},
	{0x520B, 0x30},
	{0x520C, 0x75},
	{0x520D, 0x00},
	{0x520E, 0x30},
	{0x520F, 0x75},
	{0x5210, 0x00},
	{0x5280, 0x14},
	{0x5281, 0x02},
	{0x5282, 0x02},
	{0x5283, 0x04},
	{0x5284, 0x06},
	{0x5285, 0x08},
	{0x5286, 0x0C},
	{0x5287, 0x10},
	{0x5300, 0xC5},
	{0x5301, 0xA0},
	{0x5302, 0x06},
	{0x5303, 0x0A},
	{0x5304, 0x30},
	{0x5305, 0x60},
	{0x5306, 0x90},
	{0x5307, 0xC0},
	{0x5308, 0x82},
	{0x5309, 0x00},
	{0x530A, 0x26},
	{0x530B, 0x02},
	{0x530C, 0x02},
	{0x530D, 0x00},
	{0x530E, 0x0C},
	{0x530F, 0x14},
	{0x5310, 0x1A},
	{0x5311, 0x20},
	{0x5312, 0x80},
	{0x5313, 0x4B},
	{0x5380, 0x01},
	{0x5381, 0x52},
	{0x5382, 0x00},
	{0x5383, 0x4A},
	{0x5384, 0x00},
	{0x5385, 0xB6},
	{0x5386, 0x00},
	{0x5387, 0x8D},
	{0x5388, 0x00},
	{0x5389, 0x3A},
	{0x538A, 0x00},
	{0x538B, 0xA6},
	{0x538C, 0x00},
	{0x5400, 0x0D},
	{0x5401, 0x18},
	{0x5402, 0x31},
	{0x5403, 0x5A},
	{0x5404, 0x65},
	{0x5405, 0x6F},
	{0x5406, 0x77},
	{0x5407, 0x80},
	{0x5408, 0x87},
	{0x5409, 0x8F},
	{0x540A, 0xA2},
	{0x540B, 0xB2},
	{0x540C, 0xCC},
	{0x540D, 0xE4},
	{0x540E, 0xF0},
	{0x540F, 0xA0},
	{0x5410, 0x6E},
	{0x5411, 0x06},
	{0x5480, 0x19},
	{0x5481, 0x00},
	{0x5482, 0x09},
	{0x5483, 0x12},
	{0x5484, 0x04},
	{0x5485, 0x06},
	{0x5486, 0x08},
	{0x5487, 0x0C},
	{0x5488, 0x10},
	{0x5489, 0x18},
	{0x5500, 0x02},
	{0x5501, 0x03},
	{0x5502, 0x04},
	{0x5503, 0x05},
	{0x5504, 0x06},
	{0x5505, 0x08},
	{0x5506, 0x00},
	{0x5600, 0x02},
	{0x5603, 0x40},
	{0x5604, 0x28},
	{0x5609, 0x20},
	{0x560A, 0x60},
	{0x5780, 0x3E},
	{0x5781, 0x0F},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x01},
	{0x5786, 0x00},
	{0x5787, 0x04},
	{0x5788, 0x02},
	{0x5789, 0x00},
	{0x578A, 0x01},
	{0x578B, 0x02},
	{0x578C, 0x03},
	{0x578D, 0x03},
	{0x578E, 0x08},
	{0x578F, 0x0C},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x00},
	{0x5794, 0x03},
	{0x5800, 0x03},
	{0x5801, 0x24},
	{0x5802, 0x02},
	{0x5803, 0x40},
	{0x5804, 0x34},
	{0x5805, 0x05},
	{0x5806, 0x12},
	{0x5807, 0x05},
	{0x5808, 0x03},
	{0x5809, 0x3C},
	{0x580A, 0x02},
	{0x580B, 0x40},
	{0x580C, 0x26},
	{0x580D, 0x05},
	{0x580E, 0x52},
	{0x580F, 0x06},
	{0x5810, 0x03},
	{0x5811, 0x28},
	{0x5812, 0x02},
	{0x5813, 0x40},
	{0x5814, 0x24},
	{0x5815, 0x05},
	{0x5816, 0x42},
	{0x5817, 0x06},
	{0x5818, 0x0D},
	{0x5819, 0x40},
	{0x581A, 0x04},
	{0x581B, 0x0C},
	{0x3A03, 0x4C},
	{0x3A04, 0x40},
	/* FSIN setup */
	{0x3002, 0x00},
	{0x3823, 0x30},
	{0x3824, 0x00},
	{0x3825, 0x20},
	{0x3826, 0x00},
	{0x3827, 0x04},
};

struct ov2685_scv3b4035_resolution_table_t {
	char *name;
	struct msm_camera_i2c_reg_conf *settings;
	unsigned int size; /* ARRAY_SIZE(settings) */
};

/* This table has to be in the same order as they are in the sensor lib */
static struct ov2685_scv3b4035_resolution_table_t
	ov2685_scv3b4035_resolutions[] = {
	{"2MP 30fps",  ov2685_scv3b4035_1600x1200p30_settings,
		ARRAY_SIZE(ov2685_scv3b4035_1600x1200p30_settings)},
	{"720p 60fps", ov2685_scv3b4035_720p60_settings,
		ARRAY_SIZE(ov2685_scv3b4035_720p60_settings)},
};

/* FUNCTION IS NOT USED IN THE CODE */
int32_t ov2685_scv3b4035_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = -EINVAL;
	CDBG("Power Up");

	msleep(20);

	rc = msm_sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s: msm_sensor_power_up failed\n", __func__);
		return rc;
	}

	return rc;
}

/* FIXME: Stop stream null for now, use VFE stop */
/* static void ov2685_stop_stream(struct msm_sensor_ctrl_t *s_ctrl) {} */

/* FUNCTION IS NOT USED IN THE CODE */
int32_t ov2685_scv3b4035_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("Power Down");

	msleep(20);
	msm_sensor_power_down(s_ctrl);

	return 0;
}

static struct v4l2_subdev_info ov2685_scv3b4035_subdev_info[] = {
	{
		.code       = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt        = 1,
		.order      = 0,
	},
	/* more can be supported, to be added later */
};

static const struct i2c_device_id ov2685_scv3b4035_i2c_id[] = {
	{OV2685_SCV3B4035_SENSOR_NAME,
	 (kernel_ulong_t)&ov2685_scv3b4035_s_ctrl},
	{ }
};

static int32_t msm_ov2685_scv3b4035_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov2685_scv3b4035_s_ctrl);
}

static struct i2c_driver ov2685_scv3b4035_i2c_driver = {
	.id_table = ov2685_scv3b4035_i2c_id,
	.probe  = msm_ov2685_scv3b4035_i2c_probe,
	.driver = {
		.name = OV2685_SCV3B4035_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov2685_scv3b4035_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov2685_scv3b4035_dt_match[] = {
	{.compatible = "ovti,ov2685_scv3b4035",
	 .data = &ov2685_scv3b4035_s_ctrl},
	{ }
};

MODULE_DEVICE_TABLE(of, ov2685_scv3b4035_dt_match);

static int32_t ov2685_scv3b4035_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;

	match = of_match_device(ov2685_scv3b4035_dt_match, &pdev->dev);
	if (match) {
		rc = msm_sensor_platform_probe(pdev, match->data);
	} else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	return rc;
}

static struct platform_driver ov2685_scv3b4035_platform_driver = {
	.driver = {
		.name = "ovti,ov2685_scv3b4035",
		.owner = THIS_MODULE,
		.of_match_table = ov2685_scv3b4035_dt_match,
	},
	.probe = ov2685_scv3b4035_platform_probe,
};

static int __init ov2685_scv3b4035_init_module(void)
{
	int32_t rc;

	rc = platform_driver_register(&ov2685_scv3b4035_platform_driver);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&ov2685_scv3b4035_i2c_driver);
}

static void __exit ov2685_scv3b4035_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ov2685_scv3b4035_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov2685_scv3b4035_s_ctrl);
		platform_driver_unregister(&ov2685_scv3b4035_platform_driver);
	} else {
		i2c_del_driver(&ov2685_scv3b4035_i2c_driver);
	}
	return;
}

int32_t ov2685_scv3b4035_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	enum msm_sensor_resolution_t res;

	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);

	switch (cdata->cfgtype) {

	case CFG_SET_SLAVE_INFO:
		break;

	case CFG_SLAVE_READ_I2C:
		break;

	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size ||
			conf_array.size > CCI_I2C_MAX_WRITE) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_SLAVE_WRITE_I2C_ARRAY:
		break;

	case CFG_WRITE_I2C_SEQ_ARRAY:
		break;

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(stop_setting->reg_setting,
			(void *)reg_setting, stop_setting->size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}

	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
			cdata->cfg.sensor_info.subdev_intf[i] =
				s_ctrl->sensordata->sensor_info->subdev_intf[i];
		}
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);
		break;

	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;

	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			ov2685_scv3b4035_1600x1200p30_settings,
			ARRAY_SIZE(ov2685_scv3b4035_1600x1200p30_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;

	case CFG_SET_RESOLUTION: {
		int val = 0;

		if (copy_from_user(&val,
			(void *)cdata->cfg.setting, sizeof(int))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		res = val;

		if (val > 1) {
			rc = -EFAULT;
			CDBG(">>%s:%d<<\n", __func__, __LINE__);
			CDBG(">>No Good Resolution<<\n");
			break;
		}

		CDBG("CFG_SET_RESOLUTION picking %s\n",
			ov2685_scv3b4035_resolutions[res].name);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			ov2685_scv3b4035_resolutions[res].settings,
			ov2685_scv3b4035_resolutions[res].size,
			MSM_CAMERA_I2C_BYTE_DATA);
		CDBG("%s:%d:CFG_SET_RESOLUTION res=%d, rc=%d",
			__func__, __LINE__, res, rc);
		break;
	}

	case CFG_SET_STOP_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			ov2685_scv3b4035_stop_settings,
			ARRAY_SIZE(ov2685_scv3b4035_stop_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;

	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			ov2685_scv3b4035_start_settings,
			ARRAY_SIZE(ov2685_scv3b4035_start_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;

	case CFG_SET_SATURATION:
		break;

	case CFG_SET_CONTRAST:
		break;

	case CFG_SET_SHARPNESS:
		break;

	case CFG_SET_ISO:
		break;

	case CFG_SET_EXPOSURE_COMPENSATION:
		break;

	case CFG_SET_ANTIBANDING:
		break;

	case CFG_SET_BESTSHOT_MODE:
		break;

	case CFG_SET_EFFECT:
		break;

	case CFG_SET_WHITE_BALANCE:
		break;

	case CFG_SET_AUTOFOCUS:
		break;

	case CFG_CANCEL_AUTOFOCUS:
		break;

	case CFG_SET_STREAM_TYPE: {
		enum msm_camera_stream_type_t stream_type =
					MSM_CAMERA_STREAM_INVALID;
		if (copy_from_user(&stream_type, (void *)cdata->cfg.setting,
			sizeof(enum msm_camera_stream_type_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->camera_stream_type = stream_type;
		break;
	}

	default:
		pr_err("Invalid cfgtype func %s line %d cfgtype = %d\n",
			__func__, __LINE__, (int32_t)cdata->cfgtype);
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}
#ifdef CONFIG_COMPAT

int32_t ov2685_scv3b4035_sensor_config32(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data32 *cdata = (struct sensorb_cfg_data32 *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	enum msm_sensor_resolution_t res;

	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);

	switch (cdata->cfgtype) {

	case CFG_SET_SLAVE_INFO:
		break;

	case CFG_SLAVE_READ_I2C:
		break;

	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting32 conf_array32;
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array32,
			(void *)compat_ptr(cdata->cfg.setting),
			sizeof(struct msm_camera_i2c_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		conf_array.addr_type = conf_array32.addr_type;
		conf_array.data_type = conf_array32.data_type;
		conf_array.delay = conf_array32.delay;
		conf_array.size = conf_array32.size;
		conf_array.reg_setting = compat_ptr(conf_array32.reg_setting);

		if (!conf_array.size ||
			conf_array.size > CCI_I2C_MAX_WRITE) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_SLAVE_WRITE_I2C_ARRAY:
		break;

	case CFG_WRITE_I2C_SEQ_ARRAY:
		break;

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting,
			(void *)compat_ptr(cdata->cfg.setting),
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(stop_setting->reg_setting,
			(void *)reg_setting, stop_setting->size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}

	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
			cdata->cfg.sensor_info.subdev_intf[i] =
				s_ctrl->sensordata->sensor_info->subdev_intf[i];
		}
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);
		break;

	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;

	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			ov2685_scv3b4035_1600x1200p30_settings,
			ARRAY_SIZE(ov2685_scv3b4035_1600x1200p30_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;

	case CFG_SET_RESOLUTION: {
		int val = 0;

		if (copy_from_user(&val,
			(void *)compat_ptr(cdata->cfg.setting), sizeof(int))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		res = val;

		if (val > 1) {
			rc = -EFAULT;
			CDBG(">>%s:%d<<\n", __func__, __LINE__);
			CDBG(">>No Good Resolution<<\n");
			break;
		}

		CDBG("CFG_SET_RESOLUTION picking %s\n",
			ov2685_scv3b4035_resolutions[res].name);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			ov2685_scv3b4035_resolutions[res].settings,
			ov2685_scv3b4035_resolutions[res].size,
			MSM_CAMERA_I2C_BYTE_DATA);
		CDBG("%s:%d:CFG_SET_RESOLUTION res=%d, rc=%d",
			__func__, __LINE__, res, rc);
		break;
	}

	case CFG_SET_STOP_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			ov2685_scv3b4035_stop_settings,
			ARRAY_SIZE(ov2685_scv3b4035_stop_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;

	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			ov2685_scv3b4035_start_settings,
			ARRAY_SIZE(ov2685_scv3b4035_start_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;

	case CFG_SET_SATURATION:
		break;

	case CFG_SET_CONTRAST:
		break;

	case CFG_SET_SHARPNESS:
		break;

	case CFG_SET_ISO:
		break;

	case CFG_SET_EXPOSURE_COMPENSATION:
		break;

	case CFG_SET_ANTIBANDING:
		break;

	case CFG_SET_BESTSHOT_MODE:
		break;

	case CFG_SET_EFFECT:
		break;

	case CFG_SET_WHITE_BALANCE:
		break;

	case CFG_SET_AUTOFOCUS:
		break;

	case CFG_CANCEL_AUTOFOCUS:
		break;

	case CFG_SET_STREAM_TYPE: {
		enum msm_camera_stream_type_t stream_type =
					MSM_CAMERA_STREAM_INVALID;
		if (copy_from_user(&stream_type,
			(void *)compat_ptr(cdata->cfg.setting),
			sizeof(enum msm_camera_stream_type_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->camera_stream_type = stream_type;
		break;
	}

	default:
		pr_err("Invalid cfgtype func %s line %d cfgtype = %d\n",
			__func__, __LINE__, (int32_t)cdata->cfgtype);
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}
#endif

static struct msm_sensor_fn_t ov2685_scv3b4035_sensor_func_tbl = {
	.sensor_config = ov2685_scv3b4035_sensor_config,
    #ifdef CONFIG_COMPAT
	.sensor_config32 = ov2685_scv3b4035_sensor_config32,
    #endif
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t ov2685_scv3b4035_s_ctrl = {
	.sensor_i2c_client = &ov2685_scv3b4035_sensor_i2c_client,
	.power_setting_array.power_setting = ov2685_scv3b4035_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov2685_scv3b4035_power_setting),
	.msm_sensor_mutex = &ov2685_scv3b4035_mut,
	.sensor_v4l2_subdev_info = ov2685_scv3b4035_subdev_info,
	.sensor_v4l2_subdev_info_size =
		 ARRAY_SIZE(ov2685_scv3b4035_subdev_info),
	.func_tbl = &ov2685_scv3b4035_sensor_func_tbl,
	.is_yuv = 1,
};

module_init(ov2685_scv3b4035_init_module);
module_exit(ov2685_scv3b4035_exit_module);
MODULE_DESCRIPTION("Omnivision OV2685 2MP YUV driver");
MODULE_LICENSE("GPL v2");
