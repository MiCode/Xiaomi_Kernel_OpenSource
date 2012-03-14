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
#define SENSOR_NAME "ov9726"
#define PLATFORM_DRIVER_NAME "msm_camera_ov9726"
#define ov9726_obj ov9726_##obj

DEFINE_MUTEX(ov9726_mut);
static struct msm_sensor_ctrl_t ov9726_s_ctrl;

static struct msm_camera_i2c_reg_conf ov9726_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf ov9726_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf ov9726_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf ov9726_groupoff_settings[] = {
	{0x0104, 0x00},
};

static struct msm_camera_i2c_reg_conf ov9726_prev_settings[] = {
};

static struct msm_camera_i2c_reg_conf ov9726_recommend_settings[] = {
	{0x0103, 0x01}, /* SOFTWARE_RESET */
	{0x3026, 0x00}, /* OUTPUT_SELECT01 */
	{0x3027, 0x00}, /* OUTPUT_SELECT02 */
	{0x3002, 0xe8}, /* IO_CTRL00 */
	{0x3004, 0x03}, /* IO_CTRL01 */
	{0x3005, 0xff}, /* IO_CTRL02 */
	{0x3703, 0x42},
	{0x3704, 0x10},
	{0x3705, 0x45},
	{0x3603, 0xaa},
	{0x3632, 0x2f},
	{0x3620, 0x66},
	{0x3621, 0xc0},
	{0x0340, 0x03}, /* FRAME_LENGTH_LINES_HI */
	{0x0341, 0xC1}, /* FRAME_LENGTH_LINES_LO */
	{0x0342, 0x06}, /* LINE_LENGTH_PCK_HI */
	{0x0343, 0x80}, /* LINE_LENGTH_PCK_LO */
	{0x0202, 0x03}, /* COARSE_INTEGRATION_TIME_HI */
	{0x0203, 0x43}, /* COARSE_INTEGRATION_TIME_LO */
	{0x3833, 0x04},
	{0x3835, 0x02},
	{0x4702, 0x04},
	{0x4704, 0x00}, /* DVP_CTRL01 */
	{0x4706, 0x08},
	{0x5052, 0x01},
	{0x3819, 0x6e},
	{0x3817, 0x94},
	{0x3a18, 0x00}, /* AEC_GAIN_CEILING_HI */
	{0x3a19, 0x7f}, /* AEC_GAIN_CEILING_LO */
	{0x404e, 0x7e},
	{0x3631, 0x52},
	{0x3633, 0x50},
	{0x3630, 0xd2},
	{0x3604, 0x08},
	{0x3601, 0x40},
	{0x3602, 0x14},
	{0x3610, 0xa0},
	{0x3612, 0x20},
	{0x034c, 0x05}, /* X_OUTPUT_SIZE_HI */
	{0x034d, 0x10}, /* X_OUTPUT_SIZE_LO */
	{0x034e, 0x03}, /* Y_OUTPUT_SIZE_HI */
	{0x034f, 0x28}, /* Y_OUTPUT_SIZE_LO */
	{0x0340, 0x03}, /* FRAME_LENGTH_LINES_HI */
	{0x0341, 0xC1}, /* FRAME_LENGTH_LINES_LO */
	{0x0342, 0x06}, /* LINE_LENGTH_PCK_HI */
	{0x0343, 0x80}, /* LINE_LENGTH_PCK_LO */
	{0x0202, 0x03}, /* COARSE_INTEGRATION_TIME_HI */
	{0x0203, 0x43}, /* COARSE_INTEGRATION_TIME_LO */
	{0x0303, 0x01}, /* VT_SYS_CLK_DIV_LO */
	{0x3002, 0x00}, /* IO_CTRL00 */
	{0x3004, 0x00}, /* IO_CTRL01 */
	{0x3005, 0x00}, /* IO_CTRL02 */
	{0x4801, 0x0f}, /* MIPI_CTRL01 */
	{0x4803, 0x05}, /* MIPI_CTRL03 */
	{0x4601, 0x16}, /* VFIFO_READ_CONTROL */
	{0x3014, 0x05}, /* SC_CMMN_MIPI / SC_CTRL00 */
	{0x3104, 0x80},
	{0x0305, 0x04}, /* PRE_PLL_CLK_DIV_LO */
	{0x0307, 0x64}, /* PLL_MULTIPLIER_LO */
	{0x300c, 0x02},
	{0x300d, 0x20},
	{0x300e, 0x01},
	{0x3010, 0x01},
	{0x460e, 0x81}, /* VFIFO_CONTROL00 */
	{0x0101, 0x01}, /* IMAGE_ORIENTATION */
	{0x3707, 0x14},
	{0x3622, 0x9f},
	{0x5047, 0x3D}, /* ISP_CTRL47 */
	{0x4002, 0x45}, /* BLC_CTRL02 */
	{0x5000, 0x06}, /* ISP_CTRL0 */
	{0x5001, 0x00}, /* ISP_CTRL1 */
	{0x3406, 0x00}, /* AWB_MANUAL_CTRL */
	{0x3503, 0x13}, /* AEC_ENABLE */
	{0x4005, 0x18}, /* BLC_CTRL05 */
	{0x4837, 0x21},
	{0x0100, 0x01}, /* MODE_SELECT */
	{0x3a0f, 0x64}, /* AEC_CTRL0F */
	{0x3a10, 0x54}, /* AEC_CTRL10 */
	{0x3a11, 0xc2}, /* AEC_CTRL11 */
	{0x3a1b, 0x64}, /* AEC_CTRL1B */
	{0x3a1e, 0x54}, /* AEC_CTRL1E */
	{0x3a1a, 0x05}, /* AEC_DIFF_MAX */
};

static struct v4l2_subdev_info ov9726_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array ov9726_init_conf[] = {
	{&ov9726_recommend_settings[0],
	ARRAY_SIZE(ov9726_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov9726_confs[] = {
	{&ov9726_prev_settings[0],
	ARRAY_SIZE(ov9726_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t ov9726_dimensions[] = {
	{
		.x_output = 0x510, /* 1296 */
		.y_output = 0x328, /* 808 */
		.line_length_pclk = 0x680, /* 1664 */
		.frame_length_lines = 0x3C1, /* 961 */
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csi_params ov9726_csi_params = {
	       .data_format = CSI_10BIT,
	       .lane_cnt    = 1,
	       .lane_assign = 0xe4,
	       .dpcm_scheme = 0,
	       .settle_cnt  = 7,
};

static struct msm_camera_csi_params *ov9726_csi_params_array[] = {
	&ov9726_csi_params,
};

static struct msm_sensor_output_reg_addr_t ov9726_reg_addr = {
	.x_output = 0x034c,
	.y_output = 0x034e,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t ov9726_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x9726,
};

static struct msm_sensor_exp_gain_info_t ov9726_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0204,
	.vert_offset = 6,
};

static const struct i2c_device_id ov9726_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov9726_s_ctrl},
	{ }
};

static struct i2c_driver ov9726_i2c_driver = {
	.id_table = ov9726_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov9726_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ov9726_i2c_driver);
}

static struct v4l2_subdev_core_ops ov9726_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov9726_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov9726_subdev_ops = {
	.core = &ov9726_subdev_core_ops,
	.video  = &ov9726_subdev_video_ops,
};

static struct msm_sensor_fn_t ov9726_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_write_snapshot_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
};

static struct msm_sensor_reg_t ov9726_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = ov9726_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov9726_start_settings),
	.stop_stream_conf = ov9726_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov9726_stop_settings),
	.group_hold_on_conf = ov9726_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(ov9726_groupon_settings),
	.group_hold_off_conf = ov9726_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(ov9726_groupoff_settings),
	.init_settings = &ov9726_init_conf[0],
	.init_size = ARRAY_SIZE(ov9726_init_conf),
	.mode_settings = &ov9726_confs[0],
	.output_settings = &ov9726_dimensions[0],
	.num_conf = ARRAY_SIZE(ov9726_confs),
};

static struct msm_sensor_ctrl_t ov9726_s_ctrl = {
	.msm_sensor_reg = &ov9726_regs,
	.sensor_i2c_client = &ov9726_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &ov9726_reg_addr,
	.sensor_id_info = &ov9726_id_info,
	.sensor_exp_gain_info = &ov9726_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &ov9726_csi_params_array[0],
	.msm_sensor_mutex = &ov9726_mut,
	.sensor_i2c_driver = &ov9726_i2c_driver,
	.sensor_v4l2_subdev_info = ov9726_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov9726_subdev_info),
	.sensor_v4l2_subdev_ops = &ov9726_subdev_ops,
	.func_tbl = &ov9726_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision WXGA Bayer sensor driver");
MODULE_LICENSE("GPL v2");


