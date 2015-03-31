/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 XiaoMi, Inc.
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
#include <asm/bootinfo.h>
#define S5K3M2_SENSOR_NAME "s5k3m2"
DEFINE_MSM_MUTEX(s5k3m2_mut);

static struct msm_sensor_ctrl_t s5k3m2_s_ctrl;

static struct msm_sensor_power_setting s5k3m2_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_IMG_EN,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_AF_PWDM,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},

	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
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
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info s5k3m2_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id s5k3m2_i2c_id[] = {
	{S5K3M2_SENSOR_NAME, (kernel_ulong_t)&s5k3m2_s_ctrl},
	{ }
};

static int32_t msm_s5k3m2_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &s5k3m2_s_ctrl);
}

static struct i2c_driver s5k3m2_i2c_driver = {
	.id_table = s5k3m2_i2c_id,
	.probe  = msm_s5k3m2_i2c_probe,
	.driver = {
		.name = S5K3M2_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k3m2_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id s5k3m2_dt_match[] = {
	{.compatible = "qcom,s5k3m2", .data = &s5k3m2_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, s5k3m2_dt_match);

static struct platform_driver s5k3m2_platform_driver = {
	.driver = {
		.name = "qcom,s5k3m2",
		.owner = THIS_MODULE,
		.of_match_table = s5k3m2_dt_match,
	},
};

static int32_t s5k3m2_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(s5k3m2_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

#define S5K3M2_WW(a,b) s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, a, b, MSM_CAMERA_I2C_WORD_DATA);
#define S5K3M2_RW(a,b) s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04+a, b, MSM_CAMERA_I2C_WORD_DATA);
static int s5k3m2_wait_otp_page(struct msm_sensor_ctrl_t *s_ctrl, u16 page)
{
	S5K3M2_WW(0x0136, 0x1800);
	S5K3M2_WW(0x0304, 0x0006);
	S5K3M2_WW(0x0306, 0x0073);
	S5K3M2_WW(0x030C, 0x0004);
	S5K3M2_WW(0x030E, 0x0064);
	S5K3M2_WW(0x0302, 0x0001);
	S5K3M2_WW(0x0300, 0x0004);
	S5K3M2_WW(0x030A, 0x0001);
	S5K3M2_WW(0x0308, 0x0008);
	S5K3M2_WW(0x0100, 0x0100);
	msleep(10);
	S5K3M2_WW(0x0A02, page);
	S5K3M2_WW(0x0A00, 0x0100);

	return 0;
}

extern uint16_t imx214_af_inf;
extern uint16_t imx214_af_mac;
static int32_t s5k3m2_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t i;
	int32_t rc = 0;
	uint16_t chipid = 0;

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
		s_ctrl->sensordata->sensor_name);
	}

	pr_info("%s: read id: %x expected id: %x\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);

	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match");
		return -ENODEV;
	}
        s_ctrl->sensordata->sensor_name = "s5k3m2";

	if(imx214_af_inf > 0) return 0;

	s5k3m2_wait_otp_page(s_ctrl, 0x1F00);
	S5K3M2_RW(0, &chipid);
	pr_info("%s OTP0:0x%04x", __func__, chipid);

	for(i = 62; i >= 30; i -= 16) {
		S5K3M2_RW(i, &chipid);
		pr_info("%s OTP0:0x%04x", __func__, chipid);
		if((chipid & 0xFF) == 0x11) {
			S5K3M2_RW(i - 14, &imx214_af_inf);
			S5K3M2_RW(i - 12, &imx214_af_mac);
			break;
		}
	}
/* infinite tolerance 15% */
        if ((imx214_af_mac - imx214_af_inf) / 16 > imx214_af_inf)
                imx214_af_inf = 0;
        else
                imx214_af_inf -= (imx214_af_mac - imx214_af_inf) / 16;

        imx214_af_mac += (imx214_af_mac - imx214_af_inf) / 5; /* add more dac for marco focusing */
        if(imx214_af_mac > 510)
                imx214_af_mac = 510;

	pr_info("%s inf:%d marco:%d ", __func__, imx214_af_inf, imx214_af_mac);
        return 0;
}

static int __init s5k3m2_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&s5k3m2_platform_driver,
		s5k3m2_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&s5k3m2_i2c_driver);
}

static void __exit s5k3m2_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (s5k3m2_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&s5k3m2_s_ctrl);
		platform_driver_unregister(&s5k3m2_platform_driver);
	} else
		i2c_del_driver(&s5k3m2_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t s5k3m2_s_ctrl = {
	.sensor_i2c_client = &s5k3m2_sensor_i2c_client,
	.power_setting_array.power_setting = s5k3m2_power_setting,
	.power_setting_array.size = ARRAY_SIZE(s5k3m2_power_setting),
	.msm_sensor_mutex = &s5k3m2_mut,
	.sensor_v4l2_subdev_info = s5k3m2_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k3m2_subdev_info),
	.sensor_match_id = s5k3m2_match_id,
};

module_init(s5k3m2_init_module);
module_exit(s5k3m2_exit_module);
MODULE_DESCRIPTION("s5k3m2");
MODULE_LICENSE("GPL v2");
