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

#define FLASH_NAME "sc628a"

static struct msm_flash_ctrl_t fctrl;
static struct i2c_driver sc628a_i2c_driver;

static struct msm_camera_i2c_reg_conf sc628a_off_setting[] = {
	{0x02, 0x00},
};

static struct msm_camera_i2c_reg_conf sc628a_low_setting[] = {
	{0x02, 0x06},
};

static struct msm_camera_i2c_reg_conf sc628a_high_setting[] = {
	{0x02, 0x49},
};

static int __exit msm_flash_i2c_remove(struct i2c_client *client)
{
	i2c_del_driver(&sc628a_i2c_driver);
	return 0;
}

static const struct i2c_device_id sc628a_i2c_id[] = {
	{FLASH_NAME, (kernel_ulong_t)&fctrl},
	{ }
};

static struct i2c_driver sc628a_i2c_driver = {
	.id_table = sc628a_i2c_id,
	.probe  = msm_flash_i2c_probe,
	.remove = __exit_p(msm_flash_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
	},
};

static int __init msm_flash_i2c_add_driver(void)
{
	CDBG("%s called\n", __func__);
	return i2c_add_driver(&sc628a_i2c_driver);
}

static struct msm_camera_i2c_client sc628a_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_flash_reg_t sc628a_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.off_setting = sc628a_off_setting,
	.off_setting_size = ARRAY_SIZE(sc628a_off_setting),
	.low_setting = sc628a_low_setting,
	.low_setting_size = ARRAY_SIZE(sc628a_low_setting),
	.high_setting = sc628a_high_setting,
	.high_setting_size = ARRAY_SIZE(sc628a_high_setting),
};

static struct msm_flash_fn_t sc628a_func_tbl = {
	.flash_led_config = msm_camera_flash_led_config,
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
};

static struct msm_flash_ctrl_t fctrl = {
	.flash_i2c_client = &sc628a_i2c_client,
	.reg_setting = &sc628a_regs,
	.func_tbl = &sc628a_func_tbl,
};

subsys_initcall(msm_flash_i2c_add_driver);
MODULE_DESCRIPTION("SC628A FLASH");
MODULE_LICENSE("GPL v2");
