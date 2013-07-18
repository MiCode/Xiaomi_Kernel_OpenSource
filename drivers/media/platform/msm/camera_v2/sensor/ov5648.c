/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#define OV5648_SENSOR_NAME "ov5648"
DEFINE_MSM_MUTEX(ov5648_mut);

static struct msm_sensor_ctrl_t ov5648_s_ctrl;

static struct msm_sensor_power_setting ov5648_power_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VDIG,
		.config_val = GPIO_OUT_LOW,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VDIG,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 30,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 30,
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
		.delay = 30,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info ov5648_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id ov5648_i2c_id[] = {
	{OV5648_SENSOR_NAME,
		(kernel_ulong_t)&ov5648_s_ctrl},
	{ }
};

static int32_t msm_ov5648_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov5648_s_ctrl);
}

static struct i2c_driver ov5648_i2c_driver = {
	.id_table = ov5648_i2c_id,
	.probe  = msm_ov5648_i2c_probe,
	.driver = {
		.name = OV5648_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov5648_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static struct msm_sensor_ctrl_t ov5648_s_ctrl = {
	.sensor_i2c_client = &ov5648_sensor_i2c_client,
	.power_setting_array.power_setting = ov5648_power_setting,
	.power_setting_array.size =
			ARRAY_SIZE(ov5648_power_setting),
	.msm_sensor_mutex = &ov5648_mut,
	.sensor_v4l2_subdev_info = ov5648_subdev_info,
	.sensor_v4l2_subdev_info_size =
			ARRAY_SIZE(ov5648_subdev_info),
};

static const struct of_device_id ov5648_dt_match[] = {
	{
		.compatible = "qcom,ov5648",
		.data = &ov5648_s_ctrl
	},
	{}
};

MODULE_DEVICE_TABLE(of, ov5648_dt_match);

static struct platform_driver ov5648_platform_driver = {
	.driver = {
		.name = "qcom,ov5648",
		.owner = THIS_MODULE,
		.of_match_table = ov5648_dt_match,
	},
};

static int32_t ov5648_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	match = of_match_device(ov5648_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init ov5648_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_probe(&ov5648_platform_driver,
		ov5648_platform_probe);
	if (!rc)
		return rc;
	return i2c_add_driver(&ov5648_i2c_driver);
}

static void __exit ov5648_exit_module(void)
{
	if (ov5648_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov5648_s_ctrl);
		platform_driver_unregister(&ov5648_platform_driver);
	} else
		i2c_del_driver(&ov5648_i2c_driver);
	return;
}

module_init(ov5648_init_module);
module_exit(ov5648_exit_module);
MODULE_DESCRIPTION("ov5648");
MODULE_LICENSE("GPL v2");
