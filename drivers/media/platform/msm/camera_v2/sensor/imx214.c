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
#define IMX214_SENSOR_NAME "imx214"
DEFINE_MSM_MUTEX(imx214_mut);

static struct msm_sensor_ctrl_t imx214_s_ctrl;

static struct msm_sensor_power_setting imx214_power_setting[] = {
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

static struct v4l2_subdev_info imx214_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id imx214_i2c_id[] = {
	{IMX214_SENSOR_NAME, (kernel_ulong_t)&imx214_s_ctrl},
	{ }
};

static int32_t msm_imx214_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &imx214_s_ctrl);
}

static struct i2c_driver imx214_i2c_driver = {
	.id_table = imx214_i2c_id,
	.probe  = msm_imx214_i2c_probe,
	.driver = {
		.name = IMX214_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx214_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id imx214_dt_match[] = {
	{.compatible = "qcom,imx214", .data = &imx214_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, imx214_dt_match);

static struct platform_driver imx214_platform_driver = {
	.driver = {
		.name = "qcom,imx214",
		.owner = THIS_MODULE,
		.of_match_table = imx214_dt_match,
	},
};

static int32_t imx214_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(imx214_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int imx214_wait_otp_page(struct msm_sensor_ctrl_t *s_ctrl, u16 page)
{
        int i;
        uint16_t byte = 0;

        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x0A02, page, MSM_CAMERA_I2C_BYTE_DATA);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
        udelay(10);
        for(i = 0; i < 10; i++) {
	        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x0A01, &byte, MSM_CAMERA_I2C_BYTE_DATA);
                if((byte & 0x1) == 1) return 1;
		else if(byte == 0x5d) return 0;
                udelay(10);
                pr_info("%s byte:%d", __func__, byte);
        }
        return 0;
}

uint16_t imx214_af_inf = 0;
uint16_t imx214_af_mac = 0;
uint16_t imx214_af_12m = 0;
uint16_t lc8214_offset = 0;
uint16_t lc8214_bias   = 0;
int16_t lc8214_af_inf  = 0;
int16_t lc8214_af_mac  = 0;
int8_t  g_x5_vendor    = 0;

static int32_t imx214_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t i;
	int32_t rc = 0;
	uint16_t chipid = 0;
	uint16_t lensvcmid = 0;
	uint16_t idd[12];
	uint16_t sid = s_ctrl->sensor_i2c_client->cci_client->sid;
	uint16_t addr_type = s_ctrl->sensor_i2c_client->addr_type;

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

	if(imx214_af_inf > 0)
		return rc;

	if (get_hw_version_major() == 5) {
		g_x5_vendor = 1;
		s_ctrl->sensordata->sensor_name = "imx224";
		s_ctrl->sensor_i2c_client->cci_client->sid = 0x50;
		s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x00, &chipid,        MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x08, &lensvcmid,     MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x10, &imx214_af_inf, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x12, &imx214_af_mac, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x14, &imx214_af_12m, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->cci_client->sid = sid;
		s_ctrl->sensor_i2c_client->addr_type = addr_type;
		if (chipid == 0x0D03) /* Semco Module */ {
			s_ctrl->sensordata->sensor_name = "imx225";
			g_x5_vendor = 2;
		} else if (chipid == 0x1601) /* Primax Module */ {
			s_ctrl->sensordata->sensor_name = "imx226";
			g_x5_vendor = 3;
		}
		if (get_hw_version_minor() > 2) {
			imx214_af_inf -= (imx214_af_mac - imx214_af_inf) / 16;
			imx214_af_mac += (imx214_af_mac - imx214_af_inf) / 8;
		}

		pr_info("%s x5 module:0x%04x|%04x  af inf:%d 12m:%d mac:%d", __func__, chipid, lensvcmid, imx214_af_inf, imx214_af_12m, imx214_af_mac);
		return 0;
	}

/* Read OTP */
#define IMX214_READB(a,b) s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04+a, b, MSM_CAMERA_I2C_BYTE_DATA);
#define IMX214_READW(a,b) s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04+a, b, MSM_CAMERA_I2C_WORD_DATA);

	for(i=2; i>=0 ;i--) {
		if( imx214_wait_otp_page(s_ctrl, i) == 0) continue;
		IMX214_READB(0x27, &chipid);
		if(chipid != 0x11) continue;
/* pass flag ok, read af calibration data */
		IMX214_READW(0x10, &imx214_af_inf);
		IMX214_READW(0x12, &imx214_af_mac);
		for(rc=0; rc<=11; rc++)
			IMX214_READB(rc, &(idd[rc]));
		break;
	}
	if(i < 0) {
/* maybe old otp format */
		IMX214_READW(0x14, &imx214_af_inf);
		IMX214_READW(0x16, &imx214_af_mac);
	}
	pr_info("imx214 %02x%02x %02d.%02d.%02d %02d:%02d:%02d, sn:%02x %02x %02x %02x [%d,%d]",
		idd[0], idd[1], idd[2], idd[3], idd[4], idd[5], idd[6], idd[7], idd[8], idd[9], idd[10], idd[11],
		imx214_af_inf, imx214_af_mac);

	if(((idd[0] == 0xD) && (idd[1] == 0x4)) || ((idd[0] == 0xD) && (idd[1] == 0x1))) { /* 0D04 primax, 0D01 liteon on-semi module */
		s_ctrl->sensordata->sensor_name = "imx215";
		IMX214_READB(0x14, &lc8214_offset);
		IMX214_READB(0x15, &lc8214_bias);
		lc8214_af_inf = (int16_t)imx214_af_inf;
		lc8214_af_mac = (int16_t)imx214_af_mac;
		lc8214_af_inf = 512 - (lc8214_af_inf / 64);
		lc8214_af_mac = 512 - (lc8214_af_mac / 64);

		pr_info("imx214 ON 0x%02x 0x%02x [%d,%d]", lc8214_offset, lc8214_bias, lc8214_af_inf, lc8214_af_mac);
		imx214_af_mac = lc8214_af_mac;
		imx214_af_inf = lc8214_af_inf;
		if ((imx214_af_mac - imx214_af_inf) / 16 > imx214_af_inf)
			imx214_af_inf = 0;
		else
			imx214_af_inf -= (imx214_af_mac - imx214_af_inf) / 16;

		imx214_af_mac += (imx214_af_mac - imx214_af_inf) / 5; /* add more dac for marco focusing */
		pr_info("%s inf:%d mac:%d i:%d", __func__, imx214_af_inf, imx214_af_mac, i);
		return 0;
	}

/* read otp failed, use default value*/
	if(imx214_af_mac == 0) {
		imx214_af_mac = 323;
		pr_info("%s use default mac", __func__);
	}
/* infinite tolerance 15% */
	if ((imx214_af_mac - imx214_af_inf) / 16 > imx214_af_inf)
		imx214_af_inf = 0;
	else
		imx214_af_inf -= (imx214_af_mac - imx214_af_inf) / 16;

	imx214_af_mac += (imx214_af_mac - imx214_af_inf) / 5; /* add more dac for marco focusing */
	if(imx214_af_mac > 510)
		imx214_af_mac = 510;

	pr_info("%s inf:%d mac:%d i:%d", __func__, imx214_af_inf, imx214_af_mac, i);

        return 0;
}

static int __init imx214_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&imx214_platform_driver,
		imx214_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&imx214_i2c_driver);
}

static void __exit imx214_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (imx214_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&imx214_s_ctrl);
		platform_driver_unregister(&imx214_platform_driver);
	} else
		i2c_del_driver(&imx214_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t imx214_s_ctrl = {
	.sensor_i2c_client = &imx214_sensor_i2c_client,
	.power_setting_array.power_setting = imx214_power_setting,
	.power_setting_array.size = ARRAY_SIZE(imx214_power_setting),
	.msm_sensor_mutex = &imx214_mut,
	.sensor_v4l2_subdev_info = imx214_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx214_subdev_info),
	.sensor_match_id = imx214_match_id,
};

module_init(imx214_init_module);
module_exit(imx214_exit_module);
MODULE_DESCRIPTION("imx214");
MODULE_LICENSE("GPL v2");
