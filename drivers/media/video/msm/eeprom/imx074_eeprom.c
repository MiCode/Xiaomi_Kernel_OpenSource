/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include "msm_camera_eeprom.h"
#include "msm_camera_i2c.h"

DEFINE_MUTEX(imx074_eeprom_mutex);
static struct msm_eeprom_ctrl_t imx074_eeprom_t;

static const struct i2c_device_id imx074_eeprom_i2c_id[] = {
	{"imx074_eeprom", (kernel_ulong_t)&imx074_eeprom_t},
	{ }
};

static struct i2c_driver imx074_eeprom_i2c_driver = {
	.id_table = imx074_eeprom_i2c_id,
	.probe  = msm_eeprom_i2c_probe,
	.remove = __exit_p(imx074_eeprom_i2c_remove),
	.driver = {
		.name = "imx074_eeprom",
	},
};

static int __init imx074_eeprom_i2c_add_driver(void)
{
	int rc = 0;
	rc = i2c_add_driver(imx074_eeprom_t.i2c_driver);
	return rc;
}

static struct v4l2_subdev_core_ops imx074_eeprom_subdev_core_ops = {
	.ioctl = msm_eeprom_subdev_ioctl,
};

static struct v4l2_subdev_ops imx074_eeprom_subdev_ops = {
	.core = &imx074_eeprom_subdev_core_ops,
};

uint8_t imx074_wbcalib_data[6];
struct msm_calib_wb imx074_wb_data;

static struct msm_camera_eeprom_info_t imx074_calib_supp_info = {
	{FALSE, 0, 0, 1},
	{TRUE, 6, 0, 1024},
	{FALSE, 0, 0, 1},
	{FALSE, 0, 0, 1},
};

static struct msm_camera_eeprom_read_t imx074_eeprom_read_tbl[] = {
	{0x10, &imx074_wbcalib_data[0], 6, 0},
};


static struct msm_camera_eeprom_data_t imx074_eeprom_data_tbl[] = {
	{&imx074_wb_data, sizeof(struct msm_calib_wb)},
};

static void imx074_format_wbdata(void)
{
	imx074_wb_data.r_over_g = (uint16_t)(imx074_wbcalib_data[0] << 8) |
		imx074_wbcalib_data[1];
	imx074_wb_data.b_over_g = (uint16_t)(imx074_wbcalib_data[2] << 8) |
		imx074_wbcalib_data[3];
	imx074_wb_data.gr_over_gb = (uint16_t)(imx074_wbcalib_data[4] << 8) |
		imx074_wbcalib_data[5];
}

void imx074_format_calibrationdata(void)
{
	imx074_format_wbdata();
}
static struct msm_eeprom_ctrl_t imx074_eeprom_t = {
	.i2c_driver = &imx074_eeprom_i2c_driver,
	.i2c_addr = 0xA4,
	.eeprom_v4l2_subdev_ops = &imx074_eeprom_subdev_ops,

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	},

	.eeprom_mutex = &imx074_eeprom_mutex,

	.func_tbl = {
		.eeprom_init = NULL,
		.eeprom_release = NULL,
		.eeprom_get_info = msm_camera_eeprom_get_info,
		.eeprom_get_data = msm_camera_eeprom_get_data,
		.eeprom_set_dev_addr = NULL,
		.eeprom_format_data = imx074_format_calibrationdata,
	},
	.info = &imx074_calib_supp_info,
	.info_size = sizeof(struct msm_camera_eeprom_info_t),
	.read_tbl = imx074_eeprom_read_tbl,
	.read_tbl_size = ARRAY_SIZE(imx074_eeprom_read_tbl),
	.data_tbl = imx074_eeprom_data_tbl,
	.data_tbl_size = ARRAY_SIZE(imx074_eeprom_data_tbl),
};

subsys_initcall(imx074_eeprom_i2c_add_driver);
MODULE_DESCRIPTION("IMX074 EEPROM");
MODULE_LICENSE("GPL v2");
