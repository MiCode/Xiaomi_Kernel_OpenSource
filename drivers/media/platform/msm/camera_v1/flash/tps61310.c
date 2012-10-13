/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
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
#include <linux/export.h>
#include "msm_flash.h"

#define FLASH_NAME "tps61310"

static struct msm_flash_ctrl_t fctrl;
static struct i2c_driver tps61310_i2c_driver;

static struct msm_camera_i2c_reg_conf tps61310_init_setting[] = {
	{0x01, 0x00},
};

static struct msm_camera_i2c_reg_conf tps61310_off_setting[] = {
	{0x01, 0x00},
};

static struct msm_camera_i2c_reg_conf tps61310_low_setting[] = {
	{0x01, 0x86},
};

static struct msm_camera_i2c_reg_conf tps61310_high_setting[] = {
	{0x01, 0x8B},
};

static int __exit msm_flash_i2c_remove(struct i2c_client *client)
{
	i2c_del_driver(&tps61310_i2c_driver);
	return 0;
}

static const struct i2c_device_id tps61310_i2c_id[] = {
	{FLASH_NAME, (kernel_ulong_t)&fctrl},
	{ }
};

static struct i2c_driver tps61310_i2c_driver = {
	.id_table = tps61310_i2c_id,
	.probe  = msm_flash_i2c_probe,
	.remove = __exit_p(msm_flash_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
	},
};

static int __init msm_flash_i2c_add_driver(void)
{
	CDBG("%s called\n", __func__);
	return i2c_add_driver(&tps61310_i2c_driver);
}

static struct msm_camera_i2c_client tps61310_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_flash_reg_t tps61310_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.init_setting = tps61310_init_setting,
	.init_setting_size = ARRAY_SIZE(tps61310_init_setting),
	.off_setting = tps61310_off_setting,
	.off_setting_size = ARRAY_SIZE(tps61310_off_setting),
	.low_setting = tps61310_low_setting,
	.low_setting_size = ARRAY_SIZE(tps61310_low_setting),
	.high_setting = tps61310_high_setting,
	.high_setting_size = ARRAY_SIZE(tps61310_high_setting),
};

static struct msm_flash_fn_t tps61310_func_tbl = {
	.flash_led_config = msm_camera_flash_led_config,
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
};

static struct msm_flash_ctrl_t fctrl = {
	.flash_i2c_client = &tps61310_i2c_client,
	.reg_setting = &tps61310_regs,
	.func_tbl = &tps61310_func_tbl,
};

subsys_initcall(msm_flash_i2c_add_driver);
MODULE_DESCRIPTION("TPS61310 FLASH");
MODULE_LICENSE("GPL v2");
