/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#define SENSOR_NAME "imx091"
#define PLATFORM_DRIVER_NAME "msm_camera_imx091"
#define imx091_obj imx091_##obj

DEFINE_MUTEX(imx091_mut);
static struct msm_sensor_ctrl_t imx091_s_ctrl;

static struct msm_camera_i2c_reg_conf imx091_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx091_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx091_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx091_groupoff_settings[] = {
	{0x0104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx091_prev_settings[] = {
	/* 30fps 1/2 * 1/2 */
	/* PLL setting */
	{0x0305, 0x02}, /* pre_pll_clk_div[7:0] */
	{0x0307, 0x2F}, /* pll_multiplier[7:0] */
	{0x30A4, 0x02},
	{0x303C, 0x4B},
	/* mode setting */
	{0x0340, 0x06}, /* frame_length_lines[15:8] */
	{0x0341, 0x5A}, /* frame_length_lines[7:0] */
	{0x0342, 0x12}, /* line_length_pck[15:8] */
	{0x0343, 0x0C}, /* line_length_pck[7:0] */
	{0x0344, 0x00}, /* x_addr_start[15:8] */
	{0x0345, 0x08}, /* x_addr_start[7:0] */
	{0x0346, 0x00}, /* y_addr_start[15:8] */
	{0x0347, 0x30}, /* y_addr_start[7:0] */
	{0x0348, 0x10}, /* x_addr_end[15:8] */
	{0x0349, 0x77}, /* x_addr_end[7:0] */
	{0x034A, 0x0C}, /* y_addr_end[15:8] */
	{0x034B, 0x5F}, /* y_addr_end[7:0] */
	{0x034C, 0x08}, /* x_output_size[15:8] */
	{0x034D, 0x38}, /* x_output_size[7:0] */
	{0x034E, 0x06}, /* y_output_size[15:8] */
	{0x034F, 0x18}, /* y_output_size[7:0] */
	{0x0381, 0x01}, /* x_even_inc[3:0] */
	{0x0383, 0x03}, /* x_odd_inc[3:0] */
	{0x0385, 0x01}, /* y_even_inc[7:0] */
	{0x0387, 0x03}, /* y_odd_inc[7:0] */
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x01},
	{0x3064, 0x12},
	{0x309B, 0x28},
	{0x309E, 0x00},
	{0x30D5, 0x09},
	{0x30D6, 0x01},
	{0x30D7, 0x01},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DE, 0x02},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x310A, 0x0A},
	{0x315C, 0x99},
	{0x315D, 0x98},
	{0x316E, 0x9A},
	{0x316F, 0x99},
	{0x3318, 0x73},
};

static struct msm_camera_i2c_reg_conf imx091_snap_settings[] = {
	/* full size */
	/* PLL setting */
	{0x0305, 0x02}, /* pre_pll_clk_div[7:0] */
	{0x0307, 0x2B}, /* pll_multiplier[7:0] */
	{0x30A4, 0x02},
	{0x303C, 0x4B},
	/* mode setting */
	{0x0340, 0x0C}, /* frame_length_lines[15:8] */
	{0x0341, 0x8C}, /* frame_length_lines[7:0] */
	{0x0342, 0x12}, /* line_length_pck[15:8] */
	{0x0343, 0x0C}, /* line_length_pck[7:0] */
	{0x0344, 0x00}, /* x_addr_start[15:8] */
	{0x0345, 0x08}, /* x_addr_start[7:0] */
	{0x0346, 0x00}, /* y_addr_start[15:8] */
	{0x0347, 0x30}, /* y_addr_start[7:0] */
	{0x0348, 0x10}, /* x_addr_end[15:8] */
	{0x0349, 0x77}, /* x_addr_end[7:0] */
	{0x034A, 0x0C}, /* y_addr_end[15:8] */
	{0x034B, 0x5F}, /* y_addr_end[7:0] */
	{0x034C, 0x10}, /* x_output_size[15:8] */
	{0x034D, 0x70}, /* x_output_size[7:0] */
	{0x034E, 0x0C}, /* y_output_size[15:8] */
	{0x034F, 0x30}, /* y_output_size[7:0] */
	{0x0381, 0x01}, /* x_even_inc[3:0] */
	{0x0383, 0x01}, /* x_odd_inc[3:0] */
	{0x0385, 0x01}, /* y_even_inc[7:0] */
	{0x0387, 0x01}, /* y_odd_inc[7:0] */
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x3064, 0x12},
	{0x309B, 0x20},
	{0x309E, 0x00},
	{0x30D5, 0x00},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DE, 0x00},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x310A, 0x0A},
	{0x315C, 0x99},
	{0x315D, 0x98},
	{0x316E, 0x9A},
	{0x316F, 0x99},
	{0x3318, 0x64},
};

static struct msm_camera_i2c_reg_conf imx091_recommend_settings[] = {
	/* global setting */
	{0x3087, 0x53},
	{0x309D, 0x94},
	{0x30A1, 0x08},
	{0x30C7, 0x00},
	{0x3115, 0x0E},
	{0x3118, 0x42},
	{0x311D, 0x34},
	{0x3121, 0x0D},
	{0x3212, 0xF2},
	{0x3213, 0x0F},
	{0x3215, 0x0F},
	{0x3217, 0x0B},
	{0x3219, 0x0B},
	{0x321B, 0x0D},
	{0x321D, 0x0D},
	/* black level setting */
	{0x3032, 0x40},
};

static struct v4l2_subdev_info imx091_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx091_init_conf[] = {
	{&imx091_recommend_settings[0],
	ARRAY_SIZE(imx091_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array imx091_confs[] = {
	{&imx091_snap_settings[0],
	ARRAY_SIZE(imx091_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx091_prev_settings[0],
	ARRAY_SIZE(imx091_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t imx091_dimensions[] = {
	{
	/* full size */
		.x_output = 0x1070, /* 4208 */
		.y_output = 0x0C30, /* 3120 */
		.line_length_pclk = 0x120C, /* 4620 */
		.frame_length_lines = 0x0C8C, /* 3212 */
		.vt_pixel_clk = 206400000,
		.op_pixel_clk = 206400000,
		.binning_factor = 1,
	},
	{
	/* 30 fps 1/2 * 1/2 */
		.x_output = 0x0838, /* 2104 */
		.y_output = 0x0618, /* 1560 */
		.line_length_pclk = 0x120C, /* 4620 */
		.frame_length_lines = 0x065A, /* 1626 */
		.vt_pixel_clk = 225600000,
		.op_pixel_clk = 225600000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csid_vc_cfg imx091_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
	{2, CSI_RESERVED_DATA_0, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params imx091_csi_params = {
	.csid_params = {
		.lane_cnt = 4,
		.lut_params = {
			.num_cid = ARRAY_SIZE(imx091_cid_cfg),
			.vc_cfg = imx091_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 4,
		.settle_cnt = 0x12,
	},
};

static struct msm_camera_csi2_params *imx091_csi_params_array[] = {
	&imx091_csi_params,
	&imx091_csi_params,
};

static struct msm_sensor_output_reg_addr_t imx091_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t imx091_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x0091,
};

static struct msm_sensor_exp_gain_info_t imx091_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0204,
	.vert_offset = 5,
};

static const struct i2c_device_id imx091_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx091_s_ctrl},
	{ }
};

static struct i2c_driver imx091_i2c_driver = {
	.id_table = imx091_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx091_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};


static int __init imx091_sensor_init_module(void)
{
	return i2c_add_driver(&imx091_i2c_driver);
}

static struct v4l2_subdev_core_ops imx091_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx091_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx091_subdev_ops = {
	.core = &imx091_subdev_core_ops,
	.video  = &imx091_subdev_video_ops,
};

static struct msm_sensor_fn_t imx091_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_write_snapshot_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t imx091_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx091_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx091_start_settings),
	.stop_stream_conf = imx091_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx091_stop_settings),
	.group_hold_on_conf = imx091_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx091_groupon_settings),
	.group_hold_off_conf = imx091_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(imx091_groupoff_settings),
	.init_settings = &imx091_init_conf[0],
	.init_size = ARRAY_SIZE(imx091_init_conf),
	.mode_settings = &imx091_confs[0],
	.output_settings = &imx091_dimensions[0],
	.num_conf = ARRAY_SIZE(imx091_confs),
};

static struct msm_sensor_ctrl_t imx091_s_ctrl = {
	.msm_sensor_reg = &imx091_regs,
	.sensor_i2c_client = &imx091_sensor_i2c_client,
	.sensor_i2c_addr = 0x34,
	.sensor_output_reg_addr = &imx091_reg_addr,
	.sensor_id_info = &imx091_id_info,
	.sensor_exp_gain_info = &imx091_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &imx091_csi_params_array[0],
	.msm_sensor_mutex = &imx091_mut,
	.sensor_i2c_driver = &imx091_i2c_driver,
	.sensor_v4l2_subdev_info = imx091_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx091_subdev_info),
	.sensor_v4l2_subdev_ops = &imx091_subdev_ops,
	.func_tbl = &imx091_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(imx091_sensor_init_module);
MODULE_DESCRIPTION("SONY 12MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
