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
#define SENSOR_NAME "mt9e013"
#define PLATFORM_DRIVER_NAME "msm_camera_mt9e013"
#define mt9e013_obj mt9e013_##obj

DEFINE_MUTEX(mt9e013_mut);
static struct msm_sensor_ctrl_t mt9e013_s_ctrl;

static struct msm_camera_i2c_reg_conf mt9e013_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf mt9e013_groupoff_settings[] = {
	{0x0104, 0x00},
};

static struct msm_camera_i2c_reg_conf mt9e013_prev_settings[] = {
	/*Output Size (1632x1224)*/
	{0x0344, 0x0008},/*X_ADDR_START*/
	{0x0348, 0x0CC9},/*X_ADDR_END*/
	{0x0346, 0x0008},/*Y_ADDR_START*/
	{0x034A, 0x0999},/*Y_ADDR_END*/
	{0x034C, 0x0660},/*X_OUTPUT_SIZE*/
	{0x034E, 0x04C8},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFCB0},/*DATAPATH_SELECT*/
	{0x3040, 0x04C3},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0002},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x1018},/*LINE_LENGTH_PCK*/
	{0x0340, 0x055B},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x0557},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x0846},/*FINE_INTEGRATION_TIME_*/
	{0x3010, 0x0130},/*FINE_CORRECTION*/
};

static struct msm_camera_i2c_reg_conf mt9e013_snap_settings[] = {
	/*Output Size (3264x2448)*/
	{0x0344, 0x0000},/*X_ADDR_START */
	{0x0348, 0x0CCF},/*X_ADDR_END*/
	{0x0346, 0x0000},/*Y_ADDR_START */
	{0x034A, 0x099F},/*Y_ADDR_END*/
	{0x034C, 0x0CD0},/*X_OUTPUT_SIZE*/
	{0x034E, 0x09A0},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFC80},/*DATAPATH_SELECT*/
	{0x3040, 0x0041},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0000},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x13F8},/*LINE_LENGTH_PCK*/
	{0x0340, 0x0A2F},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x0A1F},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x03F6},/*FINE_INTEGRATION_TIME_ */
	{0x3010, 0x0078},/*FINE_CORRECTION*/
};

static struct msm_camera_i2c_reg_conf mt9e013_hfr60_settings[] = {
	{0x0300, 0x0005},/*VT_PIX_CLK_DIV*/
	{0x0302, 0x0001},/*VT_SYS_CLK_DIV*/
	{0x0304, 0x0002},/*PRE_PLL_CLK_DIV*/
	{0x0306, 0x0029},/*PLL_MULTIPLIER*/
	{0x0308, 0x000A},/*OP_PIX_CLK_DIV*/
	{0x030A, 0x0001},/*OP_SYS_CLK_DIV*/
	{0x0344, 0x0008},/*X_ADDR_START*/
	{0x0348, 0x0685},/*X_ADDR_END*/
	{0x0346, 0x013a},/*Y_ADDR_START*/
	{0x034A, 0x055B},/*Y_ADDR_END*/
	{0x034C, 0x0340},/*X_OUTPUT_SIZE*/
	{0x034E, 0x0212},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFC80},/*DATAPATH_SELECT*/
	{0x3040, 0x00C3},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0000},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x0970},/*LINE_LENGTH_PCK*/
	{0x0340, 0x02A1},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x02A1},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x03F6},/*FINE_INTEGRATION_TIME_*/
	{0x3010, 0x0078},/*FINE_CORRECTION*/
};

static struct msm_camera_i2c_reg_conf mt9e013_hfr90_settings[] = {
	{0x0300, 0x0005},/*VT_PIX_CLK_DIV*/
	{0x0302, 0x0001},/*VT_SYS_CLK_DIV*/
	{0x0304, 0x0002},/*PRE_PLL_CLK_DIV*/
	{0x0306, 0x003D},/*PLL_MULTIPLIER*/
	{0x0308, 0x000A},/*OP_PIX_CLK_DIV*/
	{0x030A, 0x0001},/*OP_SYS_CLK_DIV*/
	{0x0344, 0x0008},/*X_ADDR_START*/
	{0x0348, 0x0685},/*X_ADDR_END*/
	{0x0346, 0x013a},/*Y_ADDR_START*/
	{0x034A, 0x055B},/*Y_ADDR_END*/
	{0x034C, 0x0340},/*X_OUTPUT_SIZE*/
	{0x034E, 0x0212},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFC80},/*DATAPATH_SELECT*/
	{0x3040, 0x00C3},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0000},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x0970},/*LINE_LENGTH_PCK*/
	{0x0340, 0x02A1},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x02A1},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x03F6},/*FINE_INTEGRATION_TIME_*/
	{0x3010, 0x0078},/*FINE_CORRECTION*/
};

static struct msm_camera_i2c_reg_conf mt9e013_hfr120_settings[] = {
	{0x0300, 0x0005},/*VT_PIX_CLK_DIV*/
	{0x0302, 0x0001},/*VT_SYS_CLK_DIV*/
	{0x0304, 0x0002},/*PRE_PLL_CLK_DIV*/
	{0x0306, 0x0052},/*PLL_MULTIPLIER*/
	{0x0308, 0x000A},/*OP_PIX_CLK_DIV*/
	{0x030A, 0x0001},/*OP_SYS_CLK_DIV*/
	{0x0344, 0x0008},/*X_ADDR_START*/
	{0x0348, 0x0685},/*X_ADDR_END*/
	{0x0346, 0x013a},/*Y_ADDR_START*/
	{0x034A, 0x055B},/*Y_ADDR_END*/
	{0x034C, 0x0340},/*X_OUTPUT_SIZE*/
	{0x034E, 0x0212},/*Y_OUTPUT_SIZE*/
	{0x306E, 0xFC80},/*DATAPATH_SELECT*/
	{0x3040, 0x00C3},/*READ_MODE*/
	{0x3178, 0x0000},/*ANALOG_CONTROL5*/
	{0x3ED0, 0x1E24},/*DAC_LD_4_5*/
	{0x0400, 0x0000},/*SCALING_MODE*/
	{0x0404, 0x0010},/*SCALE_M*/
	/*Timing configuration*/
	{0x0342, 0x0970},/*LINE_LENGTH_PCK*/
	{0x0340, 0x02A1},/*FRAME_LENGTH_LINES*/
	{0x0202, 0x02A1},/*COARSE_INTEGRATION_TIME*/
	{0x3014, 0x03F6},/*FINE_INTEGRATION_TIME_*/
	{0x3010, 0x0078},/*FINE_CORRECTION*/
};

static struct msm_camera_i2c_reg_conf mt9e013_recommend_settings[] = {
	/*Disable embedded data*/
	{0x3064, 0x7800},/*SMIA_TEST*/
	/*configure 2-lane MIPI*/
	{0x31AE, 0x0202},/*SERIAL_FORMAT*/
	{0x31B8, 0x0E3F},/*MIPI_TIMING_2*/
	/*set data to RAW10 format*/
	{0x0112, 0x0A0A},/*CCP_DATA_FORMAT*/
	{0x30F0, 0x800D},/*VCM CONTROL*/

	{0x3044, 0x0590},
	{0x306E, 0xFC80},
	{0x30B2, 0xC000},
	{0x30D6, 0x0800},
	{0x316C, 0xB42F},
	{0x316E, 0x869A},
	{0x3170, 0x210E},
	{0x317A, 0x010E},
	{0x31E0, 0x1FB9},
	{0x31E6, 0x07FC},
	{0x37C0, 0x0000},
	{0x37C2, 0x0000},
	{0x37C4, 0x0000},
	{0x37C6, 0x0000},
	{0x3E00, 0x0011},
	{0x3E02, 0x8801},
	{0x3E04, 0x2801},
	{0x3E06, 0x8449},
	{0x3E08, 0x6841},
	{0x3E0A, 0x400C},
	{0x3E0C, 0x1001},
	{0x3E0E, 0x2603},
	{0x3E10, 0x4B41},
	{0x3E12, 0x4B24},
	{0x3E14, 0xA3CF},
	{0x3E16, 0x8802},
	{0x3E18, 0x8401},
	{0x3E1A, 0x8601},
	{0x3E1C, 0x8401},
	{0x3E1E, 0x840A},
	{0x3E20, 0xFF00},
	{0x3E22, 0x8401},
	{0x3E24, 0x00FF},
	{0x3E26, 0x0088},
	{0x3E28, 0x2E8A},
	{0x3E30, 0x0000},
	{0x3E32, 0x8801},
	{0x3E34, 0x4029},
	{0x3E36, 0x00FF},
	{0x3E38, 0x8469},
	{0x3E3A, 0x00FF},
	{0x3E3C, 0x2801},
	{0x3E3E, 0x3E2A},
	{0x3E40, 0x1C01},
	{0x3E42, 0xFF84},
	{0x3E44, 0x8401},
	{0x3E46, 0x0C01},
	{0x3E48, 0x8401},
	{0x3E4A, 0x00FF},
	{0x3E4C, 0x8402},
	{0x3E4E, 0x8984},
	{0x3E50, 0x6628},
	{0x3E52, 0x8340},
	{0x3E54, 0x00FF},
	{0x3E56, 0x4A42},
	{0x3E58, 0x2703},
	{0x3E5A, 0x6752},
	{0x3E5C, 0x3F2A},
	{0x3E5E, 0x846A},
	{0x3E60, 0x4C01},
	{0x3E62, 0x8401},
	{0x3E66, 0x3901},
	{0x3E90, 0x2C01},
	{0x3E98, 0x2B02},
	{0x3E92, 0x2A04},
	{0x3E94, 0x2509},
	{0x3E96, 0x0000},
	{0x3E9A, 0x2905},
	{0x3E9C, 0x00FF},
	{0x3ECC, 0x00EB},
	{0x3ED0, 0x1E24},
	{0x3ED4, 0xAFC4},
	{0x3ED6, 0x909B},
	{0x3EE0, 0x2424},
	{0x3EE2, 0x9797},
	{0x3EE4, 0xC100},
	{0x3EE6, 0x0540},
	{0x3174, 0x8000},
	/* PLL settings */
	{0x0300, 0x0004},/*VT_PIX_CLK_DIV*/
	{0x0302, 0x0001},/*VT_SYS_CLK_DIV*/
	{0x0304, 0x0002},/*PRE_PLL_CLK_DIV*/
	{0x0306, 0x003A},/*PLL_MULTIPLIER*/
	{0x0308, 0x000A},/*OP_PIX_CLK_DIV*/
	{0x030A, 0x0001},/*OP_SYS_CLK_DIV*/
};

static struct v4l2_subdev_info mt9e013_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array mt9e013_init_conf[] = {
	{&mt9e013_recommend_settings[0],
	ARRAY_SIZE(mt9e013_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA}
};

static struct msm_camera_i2c_conf_array mt9e013_confs[] = {
	{&mt9e013_snap_settings[0],
	ARRAY_SIZE(mt9e013_snap_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&mt9e013_prev_settings[0],
	ARRAY_SIZE(mt9e013_prev_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&mt9e013_hfr60_settings[0],
	ARRAY_SIZE(mt9e013_hfr60_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&mt9e013_hfr90_settings[0],
	ARRAY_SIZE(mt9e013_hfr90_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&mt9e013_hfr120_settings[0],
	ARRAY_SIZE(mt9e013_hfr120_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_sensor_output_info_t mt9e013_dimensions[] = {
	{
		.x_output = 0xCD0,
		.y_output = 0x9A0,
		.line_length_pclk = 0x13F8,
		.frame_length_lines = 0xA2F,
		.vt_pixel_clk = 174000000,
		.op_pixel_clk = 174000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x660,
		.y_output = 0x4C8,
		.line_length_pclk = 0x1018,
		.frame_length_lines = 0x55B,
		.vt_pixel_clk = 174000000,
		.op_pixel_clk = 174000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x340,
		.y_output = 0x212,
		.line_length_pclk = 0x970,
		.frame_length_lines = 0x2A1,
		.vt_pixel_clk = 98400000,
		.op_pixel_clk = 98400000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x340,
		.y_output = 0x212,
		.line_length_pclk = 0x970,
		.frame_length_lines = 0x2A1,
		.vt_pixel_clk = 146400000,
		.op_pixel_clk = 146400000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x340,
		.y_output = 0x212,
		.line_length_pclk = 0x970,
		.frame_length_lines = 0x2A1,
		.vt_pixel_clk = 196800000,
		.op_pixel_clk = 196800000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csi_params mt9e013_csi_params = {
	.data_format = CSI_10BIT,
	.lane_cnt    = 2,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 0x18,
};

static struct msm_camera_csi_params *mt9e013_csi_params_array[] = {
	&mt9e013_csi_params,
	&mt9e013_csi_params,
	&mt9e013_csi_params,
	&mt9e013_csi_params,
	&mt9e013_csi_params,
};

static struct msm_sensor_output_reg_addr_t mt9e013_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t mt9e013_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x4B00,
};

static struct msm_sensor_exp_gain_info_t mt9e013_exp_gain_info = {
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x305E,
	.vert_offset = 0,
};

static int32_t mt9e013_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain | 0x1000,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

static int32_t mt9e013_write_exp_snapshot_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain | 0x1000,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x301A, (0x065C|0x2), MSM_CAMERA_I2C_WORD_DATA);

	return 0;
}
static void mt9e013_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x301A, 0x8250, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x301A, 0x8650, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x301A, 0x8658, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x0104, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x301A, 0x065C, MSM_CAMERA_I2C_WORD_DATA);
}

static void mt9e013_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x301A, 0x0058, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x301A, 0x0050, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x0104, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
}

static const struct i2c_device_id mt9e013_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&mt9e013_s_ctrl},
	{ }
};

static struct i2c_driver mt9e013_i2c_driver = {
	.id_table = mt9e013_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mt9e013_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&mt9e013_i2c_driver);
}

static struct v4l2_subdev_core_ops mt9e013_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops mt9e013_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops mt9e013_subdev_ops = {
	.core = &mt9e013_subdev_core_ops,
	.video  = &mt9e013_subdev_video_ops,
};

static struct msm_sensor_fn_t mt9e013_func_tbl = {
	.sensor_start_stream = mt9e013_start_stream,
	.sensor_stop_stream = mt9e013_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = mt9e013_write_exp_gain,
	.sensor_write_snapshot_exp_gain = mt9e013_write_exp_snapshot_gain,
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
};

static struct msm_sensor_reg_t mt9e013_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.group_hold_on_conf = mt9e013_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(mt9e013_groupon_settings),
	.group_hold_off_conf = mt9e013_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(mt9e013_groupoff_settings),
	.init_settings = &mt9e013_init_conf[0],
	.init_size = ARRAY_SIZE(mt9e013_init_conf),
	.mode_settings = &mt9e013_confs[0],
	.output_settings = &mt9e013_dimensions[0],
	.num_conf = ARRAY_SIZE(mt9e013_confs),
};

static struct msm_sensor_ctrl_t mt9e013_s_ctrl = {
	.msm_sensor_reg = &mt9e013_regs,
	.sensor_i2c_client = &mt9e013_sensor_i2c_client,
	.sensor_i2c_addr = 0x6C,
	.sensor_output_reg_addr = &mt9e013_reg_addr,
	.sensor_id_info = &mt9e013_id_info,
	.sensor_exp_gain_info = &mt9e013_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &mt9e013_csi_params_array[0],
	.msm_sensor_mutex = &mt9e013_mut,
	.sensor_i2c_driver = &mt9e013_i2c_driver,
	.sensor_v4l2_subdev_info = mt9e013_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mt9e013_subdev_info),
	.sensor_v4l2_subdev_ops = &mt9e013_subdev_ops,
	.func_tbl = &mt9e013_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Aptina 8MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");


