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
#define IMX091_SENSOR_NAME "imx091"
DEFINE_MSM_MUTEX(imx091_mut);

static struct msm_sensor_ctrl_t imx091_s_ctrl;

static struct v4l2_subdev_info imx091_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_id_info_t imx091_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x0091,
};

static enum msm_camera_vreg_name_t imx091_veg_seq[] = {
	CAM_VANA,
	CAM_VAF,
	CAM_VDIG,
	CAM_VIO,
};

static struct msm_camera_power_seq_t imx091_power_seq[] = {
	{REQUEST_GPIO, 0},
	{REQUEST_VREG, 0},
	{ENABLE_VREG, 0},
	{ENABLE_GPIO, 0},
	{CONFIG_CLK, 1},
	{CONFIG_I2C_MUX, 0},
};

static const struct i2c_device_id imx091_i2c_id[] = {
	{IMX091_SENSOR_NAME, (kernel_ulong_t)&imx091_s_ctrl},
	{ }
};

static struct i2c_driver imx091_i2c_driver = {
	.id_table = imx091_i2c_id,
	.probe  = msm_sensor_bayer_i2c_probe,
	.driver = {
		.name = IMX091_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx091_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static struct v4l2_subdev_core_ops imx091_subdev_core_ops = {
	.ioctl = msm_sensor_bayer_subdev_ioctl,
	.s_power = msm_sensor_bayer_power,
};

static struct v4l2_subdev_video_ops imx091_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_bayer_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx091_subdev_ops = {
	.core = &imx091_subdev_core_ops,
	.video  = &imx091_subdev_video_ops,
};

static struct msm_sensor_fn_t imx091_func_tbl = {
	.sensor_config = msm_sensor_bayer_config,
	.sensor_power_up = msm_sensor_bayer_power_up,
	.sensor_power_down = msm_sensor_bayer_power_down,
	.sensor_get_csi_params = msm_sensor_bayer_get_csi_params,
	.sensor_read_eeprom = msm_sensor_bayer_eeprom_read,
};

static struct msm_sensor_ctrl_t imx091_s_ctrl = {
	.sensor_i2c_client = &imx091_sensor_i2c_client,
	.sensor_i2c_addr = 0x34,
	.vreg_seq = imx091_veg_seq,
	.num_vreg_seq = ARRAY_SIZE(imx091_veg_seq),
	.power_seq = &imx091_power_seq[0],
	.num_power_seq = ARRAY_SIZE(imx091_power_seq),
	.sensor_id_info = &imx091_id_info,
	.msm_sensor_mutex = &imx091_mut,
	.sensor_v4l2_subdev_info = imx091_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx091_subdev_info),
	.sensor_v4l2_subdev_ops = &imx091_subdev_ops,
	.func_tbl = &imx091_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};
