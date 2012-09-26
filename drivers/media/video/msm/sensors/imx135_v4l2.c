/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#define SENSOR_NAME "imx135"
#define PLATFORM_DRIVER_NAME "msm_camera_imx135"
#define imx135_obj imx135_##obj

DEFINE_MUTEX(imx135_mut);
static struct msm_sensor_ctrl_t imx135_s_ctrl;

static struct msm_camera_i2c_reg_conf imx135_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx135_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx135_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx135_groupoff_settings[] = {
	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx135_recommend_settings[] = {
/* Recommended global settings */
	{0x0220, 0x01},
	{0x3008, 0xB0},
	{0x320A, 0x01},
	{0x320D, 0x10},
	{0x3216, 0x2E},
	{0x3230, 0x0A},
	{0x3228, 0x05},
	{0x3229, 0x02},
	{0x322C, 0x02},
	{0x3302, 0x10},
	{0x3390, 0x45},
	{0x3409, 0x0C},
	{0x340B, 0xF5},
	{0x340C, 0x2D},
	{0x3412, 0x41},
	{0x3413, 0xAD},
	{0x3414, 0x1E},
	{0x3427, 0x04},
	{0x3480, 0x1E},
	{0x3484, 0x1E},
	{0x3488, 0x1E},
	{0x348C, 0x1E},
	{0x3490, 0x1E},
	{0x3494, 0x1E},
	{0x349C, 0x38},
	{0x34A3, 0x38},
	{0x3511, 0x8F},
	{0x3518, 0x00},
	{0x3519, 0x94},
	{0x3833, 0x20},
	{0x3893, 0x01},
	{0x38C2, 0x08},
	{0x3C09, 0x01},
	{0x4300, 0x00},
	{0x4316, 0x12},
	{0x4317, 0x22},
	{0x431A, 0x00},
	{0x4324, 0x03},
	{0x4325, 0x20},
	{0x4326, 0x03},
	{0x4327, 0x84},
	{0x4328, 0x03},
	{0x4329, 0x20},
	{0x432A, 0x03},
	{0x432B, 0x84},
	{0x4401, 0x3F},
	{0x4412, 0x3F},
	{0x4413, 0xFF},
	{0x4446, 0x3F},
	{0x4447, 0xFF},
	{0x4452, 0x00},
	{0x4453, 0xA0},
	{0x4454, 0x08},
	{0x4455, 0x00},
	{0x4458, 0x18},
	{0x4459, 0x18},
	{0x445A, 0x3F},
	{0x445B, 0x3A},
	{0x4463, 0x00},
	{0x4465, 0x00},
	{0x446E, 0x01},
/* Image Quality Settings */
/* Bypass Settings */
	{0x4203, 0x48},
/* Defect Correction Recommended Setting */
	{0x4100, 0xE0},
	{0x4102, 0x0B},
/* RGB Filter Recommended Setting */
	{0x4281, 0x22},
	{0x4282, 0x82},
	{0x4284, 0x00},
	{0x4287, 0x18},
	{0x4288, 0x00},
	{0x428B, 0x1E},
	{0x428C, 0x00},
	{0x428F, 0x08},
/* DLC/ADP Recommended Setting */
	{0x4207, 0x00},
	{0x4218, 0x02},
	{0x421B, 0x00},
	{0x4222, 0x04},
	{0x4223, 0x44},
	{0x4224, 0x46},
	{0x4225, 0xFF},
	{0x4226, 0x14},
	{0x4227, 0xF2},
	{0x4228, 0xFC},
	{0x4229, 0x60},
	{0x422A, 0xFA},
	{0x422B, 0xFE},
	{0x422C, 0xFE},
/* Color Artifact Recommended Setting */
	{0x4243, 0xAA}
};

/* IMX135 mode 1/2 HV at 24MHz */
static struct msm_camera_i2c_reg_conf imx135_prev_settings[] = {
/* Clock Setting */
	{0x011E, 0x18},
	{0x011F, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x0B},
	{0x0305, 0x03},
	{0x0306, 0x01},
	{0x0307, 0x5E},
	{0x0309, 0x05},
	{0x030B, 0x02},
	{0x030C, 0x00},
	{0x030D, 0x71},
	{0x030E, 0x01},
	{0x3A06, 0x12},
/* Mode setting */
	{0x0101, 0x00},
	{0x0105, 0x00},
	{0x0108, 0x03},
	{0x0109, 0x30},
	{0x010B, 0x32},
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x01}, /* binning_en = 1 */
	{0x0391, 0x22}, /* binning_type */
	{0x0392, 0x00}, /* binning_mode = 0 (average) */
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x4083, 0x01},
/* Size setting*/
	{0x0340, 0x0A}, /* frame_length_lines = 2680*/
	{0x0341, 0x78},
	{0x034C, 0x08},
	{0x034D, 0x38},
	{0x034E, 0x06},
	{0x034F, 0x18},
	{0x0354, 0x08},
	{0x0355, 0x38},
	{0x0356, 0x06},
	{0x0357, 0x18},
	{0x3310, 0x08},
	{0x3311, 0x38},
	{0x3312, 0x06},
	{0x3313, 0x18},
	{0x331C, 0x02},
	{0x331D, 0xC0},
	{0x33B0, 0x04},
	{0x33B1, 0x00},
	{0x33B3, 0x00},
	{0x7006, 0x04},
/* Global Timing Setting */
	{0x0830, 0x67},
	{0x0831, 0x27},
	{0x0832, 0x47},
	{0x0833, 0x27},
	{0x0834, 0x27},
	{0x0835, 0x1F},
	{0x0836, 0x87},
	{0x0837, 0x2F},
	{0x0839, 0x1F},
	{0x083A, 0x17},
	{0x083B, 0x02},
/* Integration Time Setting */
	{0x0254, 0x00},
/* Gain Setting */
	{0x0205, 0x33}
};

/* IMX135 Mode Fullsize at 24MHz */
static struct msm_camera_i2c_reg_conf imx135_snap_settings[] = {
/* Clock Setting */
	{0x011E, 0x18},
	{0x011F, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x0B},
	{0x0305, 0x03},
	{0x0306, 0x01},
	{0x0307, 0x5E},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x60}, /* pll_multiplier = 96 */
	{0x030E, 0x01},
	{0x3A06, 0x11},
/* Mode setting */
	{0x0101, 0x00},
	{0x0105, 0x00},
	{0x0108, 0x03},
	{0x0109, 0x30},
	{0x010B, 0x32},
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0391, 0x11},
	{0x0392, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x4083, 0x01},
/* Size setting */
	{0x0340, 0x0C},
	{0x0341, 0x46},
	{0x034C, 0x10},
	{0x034D, 0x70},
	{0x034E, 0x0C},
	{0x034F, 0x30},
	{0x0354, 0x10},
	{0x0355, 0x70},
	{0x0356, 0x0C},
	{0x0357, 0x30},
	{0x3310, 0x10},
	{0x3311, 0x70},
	{0x3312, 0x0C},
	{0x3313, 0x30},
	{0x331C, 0x06},
	{0x331D, 0x00},
	{0x33B0, 0x04},
	{0x33B1, 0x00},
	{0x33B3, 0x00},
	{0x7006, 0x04},
/* Global Timing Setting */
	{0x0830, 0x7F},
	{0x0831, 0x37},
	{0x0832, 0x5F},
	{0x0833, 0x37},
	{0x0834, 0x37},
	{0x0835, 0x3F},
	{0x0836, 0xC7},
	{0x0837, 0x3F},
	{0x0839, 0x1F},
	{0x083A, 0x17},
	{0x083B, 0x02},
/* Integration Time Setting */
	{0x0250, 0x0B},
/* Gain Setting */
	{0x0205, 0x33}
};


static struct msm_camera_i2c_reg_conf imx135_hdr_settings[] = {
/* Clock Setting */
	{0x011E, 0x18},
	{0x011F, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x0B},
	{0x0305, 0x03},
	{0x0306, 0x01},
	{0x0307, 0x5E},
	{0x0309, 0x05},
	{0x030B, 0x02},
	{0x030C, 0x00},
	{0x030D, 0x71},
	{0x030E, 0x01},
	{0x3A06, 0x12},
/* Mode setting */
	{0x0101, 0x00},
	{0x0105, 0x00},
	{0x0108, 0x03},
	{0x0109, 0x30},
	{0x010B, 0x32},
	{0x0112, 0x0E},
	{0x0113, 0x0A},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0391, 0x11},
	{0x0392, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x4083, 0x01},
/* Size setting */
	{0x0340, 0x0C},
	{0x0341, 0x48},
	{0x034C, 0x08},
	{0x034D, 0x38},
	{0x034E, 0x06},
	{0x034F, 0x18},
	{0x0354, 0x08},
	{0x0355, 0x38},
	{0x0356, 0x06},
	{0x0357, 0x18},
	{0x3310, 0x08},
	{0x3311, 0x38},
	{0x3312, 0x06},
	{0x3313, 0x18},
	{0x331C, 0x02},
	{0x331D, 0xA0},
	{0x33B0, 0x08},
	{0x33B1, 0x38},
	{0x33B3, 0x01},
	{0x7006, 0x04},
/* Global Timing Setting */
	{0x0830, 0x67},
	{0x0831, 0x27},
	{0x0832, 0x47},
	{0x0833, 0x27},
	{0x0834, 0x27},
	{0x0835, 0x1F},
	{0x0836, 0x87},
	{0x0837, 0x2F},
	{0x0839, 0x1F},
	{0x083A, 0x17},
	{0x083B, 0x02},
/* Integration Time Setting */
	{0x0250, 0x0B},
/* Gain Setting */
	{0x0205, 0x33}
};

static struct v4l2_subdev_info imx135_subdev_info[] = {
	{
	.code		= V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace	= V4L2_COLORSPACE_JPEG,
	.fmt		= 1,
	.order		= 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx135_init_conf[] = {
	{&imx135_recommend_settings[0],
	ARRAY_SIZE(imx135_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array imx135_confs[] = {
	{&imx135_snap_settings[0],
	ARRAY_SIZE(imx135_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx135_prev_settings[0],
	ARRAY_SIZE(imx135_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx135_hdr_settings[0],
	ARRAY_SIZE(imx135_hdr_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t imx135_dimensions[] = {
	/* RES0 snapshot(FULL SIZE) */
	{
		.x_output = 4208,
		.y_output = 3120,
		.line_length_pclk = 4572,
		.frame_length_lines = 3142,
		.vt_pixel_clk = 307200000,
		.op_pixel_clk = 307200000,
		.binning_factor = 1,
	},
	/* RES1 4:3 preview(1/2HV QTR SIZE) */
	{
		.x_output = 2104,
		.y_output = 1560,
		.line_length_pclk = 4572,
		.frame_length_lines = 2680,
		.vt_pixel_clk = 361600000,
		.op_pixel_clk = 180800000,
		.binning_factor = 1,
	},
	/* RES2 4:3 HDR movie mode */
	{
		.x_output = 2104,
		.y_output = 1560,
		.line_length_pclk = 4572,
		.frame_length_lines = 3144,
		.vt_pixel_clk = 361600000,
		.op_pixel_clk = 180800000,
		.binning_factor = 1,
	},
};

static struct msm_sensor_output_reg_addr_t imx135_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t imx135_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x0087,
};

static struct msm_sensor_exp_gain_info_t imx135_exp_gain_info = {
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x205,
	.vert_offset = 4,
};

static const struct i2c_device_id imx135_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx135_s_ctrl},
	{ }
};

static struct i2c_driver imx135_i2c_driver = {
	.id_table = imx135_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx135_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&imx135_i2c_driver);
}

static struct v4l2_subdev_core_ops imx135_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx135_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx135_subdev_ops = {
	.core = &imx135_subdev_core_ops,
	.video  = &imx135_subdev_video_ops,
};

int32_t imx135_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t offset;
	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

static struct msm_sensor_fn_t imx135_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = imx135_write_exp_gain,
	.sensor_write_snapshot_exp_gain = imx135_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t imx135_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx135_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx135_start_settings),
	.stop_stream_conf = imx135_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx135_stop_settings),
	.group_hold_on_conf = imx135_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx135_groupon_settings),
	.group_hold_off_conf = imx135_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(imx135_groupoff_settings),
	.init_settings = &imx135_init_conf[0],
	.init_size = ARRAY_SIZE(imx135_init_conf),
	.mode_settings = &imx135_confs[0],
	.output_settings = &imx135_dimensions[0],
	.num_conf = ARRAY_SIZE(imx135_confs),
};

static struct msm_sensor_ctrl_t imx135_s_ctrl = {
	.msm_sensor_reg = &imx135_regs,
	.sensor_i2c_client = &imx135_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &imx135_reg_addr,
	.sensor_id_info = &imx135_id_info,
	.sensor_exp_gain_info = &imx135_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &imx135_mut,
	.sensor_i2c_driver = &imx135_i2c_driver,
	.sensor_v4l2_subdev_info = imx135_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx135_subdev_info),
	.sensor_v4l2_subdev_ops = &imx135_subdev_ops,
	.func_tbl = &imx135_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Sony 13MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
