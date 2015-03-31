/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#define ov4688_SENSOR_NAME "ov4688"
DEFINE_MSM_MUTEX(ov4688_mut);

static struct msm_sensor_ctrl_t ov4688_s_ctrl;

static struct msm_sensor_power_setting ov4688_power_setting[] = {
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
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
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
		.delay = 10,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info ov4688_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id ov4688_i2c_id[] = {
	{ov4688_SENSOR_NAME, (kernel_ulong_t)&ov4688_s_ctrl},
	{ }
};

static struct msm_camera_i2c_reg_array otp_init_array[] = {
	{0x0103, 0x01},
	{0x3638, 0x00},
	{0x3105, 0x31},
	{0x301a, 0xf9},
	{0x3508, 0x07},
	{0x484b, 0x05},
	{0x4805, 0x03},
	{0x3601, 0x01},
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_array otp_read_array[] = {
	{0x3105, 0x11},
	{0x301a, 0xf1},
	{0x4805, 0x00},
	{0x301a, 0xf0},
	{0x3208, 0x00},
	{0x302a, 0x00},
	{0x302a, 0x00},
	{0x302a, 0x00},
	{0x302a, 0x00},
	{0x302a, 0x00},
	{0x3601, 0x00},
	{0x3638, 0x00},
	{0x3208, 0x10},
	{0x3208, 0xa0},
};

static struct msm_camera_i2c_reg_setting otp_init_setting = {
        .reg_setting = otp_init_array,
        .size = ARRAY_SIZE(otp_init_array),
        .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
        .data_type = MSM_CAMERA_I2C_BYTE_DATA,
        .delay = 10,
};

static struct msm_camera_i2c_reg_setting otp_read_setting = {
        .reg_setting = otp_read_array,
        .size = ARRAY_SIZE(otp_read_array),
        .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
        .data_type = MSM_CAMERA_I2C_BYTE_DATA,
        .delay = 0,
};

uint16_t g_wbc_r_gain = 0;
uint16_t g_wbc_g_gain = 0;
uint16_t g_wbc_b_gain = 0;
static int32_t ov4688_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
        int32_t rc = 0;
        int32_t i,j = 0;
        uint16_t chipid = 0;
        uint16_t idd[16];
	uint16_t rg, bg;
	int nR_G_gain, nB_G_gain, nG_G_gain, nBase_gain;
	/* Golden Module Ratio */
	int RG_Liteon = 224;
	int BG_Liteon = 263;
	int RG_Primax = 202;
	int BG_Primax = 244;
	int RG_Ratio_Typical = RG_Liteon;
	int BG_Ratio_Typical = BG_Liteon;

        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
                        s_ctrl->sensor_i2c_client,
                        s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
                        &chipid, MSM_CAMERA_I2C_WORD_DATA);
        if (rc < 0) {
                pr_err("%s: %s: read id failed\n", __func__,
                        s_ctrl->sensordata->sensor_name);
        }

        pr_info("%s: readed id: %x expected id: %x\n", __func__, chipid,
                s_ctrl->sensordata->slave_info->sensor_id);

        if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
                pr_err("msm_sensor_match_id chip id doesnot match\n");
                return -ENODEV;
        }

        if(g_wbc_r_gain != 0) goto update_awb_gains;

	/* read awb otp calibration */
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
                s_ctrl->sensor_i2c_client, &otp_init_setting);
        if (rc < 0)
                pr_err("%s:%d failed\n", __func__, __LINE__);

	msleep(2);

        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
                s_ctrl->sensor_i2c_client, &otp_read_setting);
        if (rc < 0)
                pr_err("%s:%d failed\n", __func__, __LINE__);
	/* check pass flag */
	idd[0] = 0;
	for(i = 0x7130; i >= 0x7110; i -= 0x10) {
	        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x3d84, 0xc0, MSM_CAMERA_I2C_BYTE_DATA);
	        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x3d88, i >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x3d89, i & 0xff, MSM_CAMERA_I2C_BYTE_DATA);
	        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x3d8a, i >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x3d8b, (i + 0xf) & 0xff, MSM_CAMERA_I2C_BYTE_DATA);
	        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x7110, 0, MSM_CAMERA_I2C_BYTE_DATA);
	        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x3d81, 1, MSM_CAMERA_I2C_BYTE_DATA);
		usleep_range(5000, 6000);
		/* read flag, 01000000b is valid */
	        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, i, &idd[0], MSM_CAMERA_I2C_BYTE_DATA);
		pr_info("ov4688 addr:0x%04x = 0x%02x", i, idd[0]);
		if(idd[0] & 0x40) break;
	}
	if((idd[0] & 0x40) == 0) {
		pr_info("%s 3 times, no otp data", __func__);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x0103, 0x01,   MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
		return 0;
	}
	/* read data */
	for(j = 1; j < 15; j++) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
                        s_ctrl->sensor_i2c_client, i+j, &idd[j], MSM_CAMERA_I2C_BYTE_DATA);
	}

	pr_info("ov4688 %02x%02x %02d.%02d.%02d [%02x:%02x] [%02x %02x]  %02x (%02x)",
                idd[1], idd[2], idd[3], idd[4], idd[5], idd[6], idd[7], idd[8], idd[9], idd[10], idd[11]);


	if(idd[1] == 0x25) { /* 0x2501 is PRIMAX 0x1502 is LITEON */
		s_ctrl->sensordata->sensor_name = "ov4689";
		RG_Ratio_Typical = RG_Primax;
		BG_Ratio_Typical = BG_Primax;
	}

	rg = ((idd[10] & 0xC0) >> 6) + (idd[6] << 2);
	bg = ((idd[10] & 0x30) >> 4) + (idd[7] << 2);

	nR_G_gain = (RG_Ratio_Typical*1000) / rg;
	nB_G_gain = (BG_Ratio_Typical*1000) / bg;
	nG_G_gain = 1000;

	if (nR_G_gain < 1000 || nB_G_gain < 1000) {
		if (nR_G_gain < nB_G_gain)
			nBase_gain = nR_G_gain;
		else
			nBase_gain = nB_G_gain;
	} else {
		nBase_gain = nG_G_gain;
	}

	g_wbc_r_gain = 0x400 * nR_G_gain / (nBase_gain);
	g_wbc_b_gain = 0x400 * nB_G_gain / (nBase_gain);
	g_wbc_g_gain = 0x400 * nG_G_gain / (nBase_gain);

	pr_info("ov4688 R:0x%04x B:0x%04x G:0x%04x by rg:%d bg:%d",
		g_wbc_r_gain, g_wbc_b_gain, g_wbc_g_gain, rg, bg);

update_awb_gains:
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x0103, 0x01,   MSM_CAMERA_I2C_BYTE_DATA);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x500c, g_wbc_r_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x500d, g_wbc_r_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x500e, g_wbc_g_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x500f, g_wbc_g_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5010, g_wbc_b_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5011, g_wbc_b_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5012, g_wbc_r_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5013, g_wbc_r_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5014, g_wbc_g_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5015, g_wbc_g_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5016, g_wbc_b_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5017, g_wbc_b_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5018, g_wbc_r_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x5019, g_wbc_r_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x501a, g_wbc_g_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x501b, g_wbc_g_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x501c, g_wbc_b_gain >> 8,   MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x501d, g_wbc_b_gain & 0xFF, MSM_CAMERA_I2C_BYTE_DATA);

	return 0;
}

static int32_t msm_ov4688_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov4688_s_ctrl);
}

static struct i2c_driver ov4688_i2c_driver = {
	.id_table = ov4688_i2c_id,
	.probe  = msm_ov4688_i2c_probe,
	.driver = {
		.name = ov4688_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov4688_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov4688_dt_match[] = {
	{.compatible = "qcom,ov4688", .data = &ov4688_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov4688_dt_match);

static struct platform_driver ov4688_platform_driver = {
	.driver = {
		.name = "qcom,ov4688",
		.owner = THIS_MODULE,
		.of_match_table = ov4688_dt_match,
	},
};

static int32_t ov4688_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(ov4688_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init ov4688_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&ov4688_platform_driver,
		ov4688_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&ov4688_i2c_driver);
}

static void __exit ov4688_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ov4688_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov4688_s_ctrl);
		platform_driver_unregister(&ov4688_platform_driver);
	} else
		i2c_del_driver(&ov4688_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t ov4688_s_ctrl = {
	.sensor_i2c_client = &ov4688_sensor_i2c_client,
	.power_setting_array.power_setting = ov4688_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov4688_power_setting),
	.msm_sensor_mutex = &ov4688_mut,
	.sensor_v4l2_subdev_info = ov4688_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov4688_subdev_info),
	.sensor_match_id = ov4688_match_id,
};

module_init(ov4688_init_module);
module_exit(ov4688_exit_module);
MODULE_DESCRIPTION("ov4688");
MODULE_LICENSE("GPL v2");
