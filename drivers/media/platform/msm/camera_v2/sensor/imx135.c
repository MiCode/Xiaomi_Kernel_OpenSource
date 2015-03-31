/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#define IMX135_SENSOR_NAME "imx135"
DEFINE_MSM_MUTEX(imx135_mut);

static struct msm_sensor_ctrl_t imx135_s_ctrl;

static struct msm_sensor_power_setting imx135_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_IMG_EN,
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
		.delay = 2,
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
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
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

static struct v4l2_subdev_info imx135_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id imx135_i2c_id[] = {
	{IMX135_SENSOR_NAME, (kernel_ulong_t)&imx135_s_ctrl},
	{ }
};

static int32_t msm_imx135_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &imx135_s_ctrl);
}

static struct i2c_driver imx135_i2c_driver = {
	.id_table = imx135_i2c_id,
	.probe  = msm_imx135_i2c_probe,
	.driver = {
		.name = IMX135_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx135_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id imx135_dt_match[] = {
	{.compatible = "qcom,imx135", .data = &imx135_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, imx135_dt_match);

static struct platform_driver imx135_platform_driver = {
	.driver = {
		.name = "qcom,imx135",
		.owner = THIS_MODULE,
		.of_match_table = imx135_dt_match,
	},
};

static int32_t imx135_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(imx135_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int imx135_wait_otp_page(struct msm_sensor_ctrl_t *s_ctrl, u16 page)
{
        int i;
        uint16_t byte = 0;

        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x3B02, page, MSM_CAMERA_I2C_BYTE_DATA);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x3B00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
        udelay(10);
        for(i = 0; i < 10; i++) {
	        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x3B01, &byte, MSM_CAMERA_I2C_BYTE_DATA);
                byte &= 1;
                if(byte == 1) break;
                udelay(10);
                pr_info("%s byte:%d", __func__, byte);
        }
        return byte;
}

uint16_t af_init_code = 0;
static int32_t imx135_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
        int32_t rc = 0;
        int32_t i = 0;
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
                pr_err("msm_sensor_match_id chip id doesnot match\n");
                return -ENODEV;
        }

	if(af_init_code > 0)
		return rc;

        if( imx135_wait_otp_page(s_ctrl, 1) == 1) {
		for(i = 0x3B24; i >= 0x3B04; i -= 0x10) {
			s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, i, &chipid, MSM_CAMERA_I2C_WORD_DATA);
                        if(chipid != 0) break;
                }
        }
        pr_info("imx135 module vendor: 0x%04x", chipid);

	// Read serial number
	chipid = 0;
        if( imx135_wait_otp_page(s_ctrl, 0) == 1) {
		for(i = 0x3B24; i <= 0x3B2B; i += 1) {
			s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, i, &chipid, MSM_CAMERA_I2C_BYTE_DATA);
			pr_info("imx135 0x%02x", chipid);
                }
        } else
		pr_info("wait page 0 failed");


        // Check OTP Write OK
	chipid = 0;
        if( imx135_wait_otp_page(s_ctrl, 16) == 1) {
                for(i = 0x3B05; i >= 0x3B04; i -= 0x1) {
                        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
                                s_ctrl->sensor_i2c_client, i, &chipid, MSM_CAMERA_I2C_BYTE_DATA);
                        if((chipid == 0x11)||(chipid == 0xEE)) goto otp_id;
                }
        }
        if( imx135_wait_otp_page(s_ctrl, 15) == 1) {
                        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
                                s_ctrl->sensor_i2c_client, 0x3B04, &chipid, MSM_CAMERA_I2C_BYTE_DATA);
                        if((chipid == 0x11)||(chipid == 0xEE)) goto otp_id;
        }
otp_id:
	if(chipid != 0x11) {
		af_init_code = 150;
		return rc;
	}

        /* read af calibration data */
	imx135_wait_otp_page(s_ctrl, 14);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
		s_ctrl->sensor_i2c_client, 0x3B04 + 12, &af_init_code, MSM_CAMERA_I2C_WORD_DATA);
        pr_info(" imx135 inf:%d ", af_init_code);

        return rc;
}

static int __init imx135_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&imx135_platform_driver,
		imx135_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&imx135_i2c_driver);
}

static void __exit imx135_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (imx135_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&imx135_s_ctrl);
		platform_driver_unregister(&imx135_platform_driver);
	} else
		i2c_del_driver(&imx135_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t imx135_s_ctrl = {
	.sensor_i2c_client = &imx135_sensor_i2c_client,
	.power_setting_array.power_setting = imx135_power_setting,
	.power_setting_array.size = ARRAY_SIZE(imx135_power_setting),
	.msm_sensor_mutex = &imx135_mut,
	.sensor_v4l2_subdev_info = imx135_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx135_subdev_info),
	.sensor_match_id = imx135_match_id,
};

module_init(imx135_init_module);
module_exit(imx135_exit_module);
MODULE_DESCRIPTION("imx135");
MODULE_LICENSE("GPL v2");
