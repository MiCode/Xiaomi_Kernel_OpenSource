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
#include <mach/gpio.h>
#include "msm_flash.h"

#define SD_INDEX 0

static struct msm_flash_ctrl_t fctrl;

static int msm_camera_flash_led(struct msm_flash_ctrl_t *fctrl,
	uint8_t led_state)
{
	int rc = 0;
	struct msm_camera_sensor_flash_external *external =
		&fctrl->flash_data->flash_src->_fsrc.ext_driver_src;

	CDBG("msm_camera_flash_led: %d\n", led_state);
	switch (led_state) {
	case MSM_CAMERA_LED_INIT:
		rc = gpio_request(external->led_en, "sgm3141");
		CDBG("MSM_CAMERA_LED_INIT: gpio_req: %d %d\n",
				external->led_en, rc);
		if (!rc)
			gpio_direction_output(external->led_en, 0);
		else
			return 0;

		rc = gpio_request(external->led_flash_en, "sgm3141");
		CDBG("MSM_CAMERA_LED_INIT: gpio_req: %d %d\n",
				external->led_flash_en, rc);
		if (!rc)
			gpio_direction_output(external->led_flash_en, 0);

			break;

	case MSM_CAMERA_LED_RELEASE:
		CDBG("MSM_CAMERA_LED_RELEASE\n");
		gpio_set_value_cansleep(external->led_en, 0);
		gpio_free(external->led_en);
		gpio_set_value_cansleep(external->led_flash_en, 0);
		gpio_free(external->led_flash_en);
		break;

	case MSM_CAMERA_LED_OFF:
		CDBG("MSM_CAMERA_LED_OFF\n");
		gpio_set_value_cansleep(external->led_en, 0);
		gpio_set_value_cansleep(external->led_flash_en, 0);
		break;

	case MSM_CAMERA_LED_LOW:
		CDBG("MSM_CAMERA_LED_LOW\n");
		gpio_set_value_cansleep(external->led_en, 1);
		gpio_set_value_cansleep(external->led_flash_en, 1);
		break;

	case MSM_CAMERA_LED_HIGH:
		CDBG("MSM_CAMERA_LED_HIGH\n");
		gpio_set_value_cansleep(external->led_en, 1);
		gpio_set_value_cansleep(external->led_flash_en, 1);
		break;

	default:
		rc = -EFAULT;
		break;
	}

	return rc;
}

static int __init msm_flash_i2c_add_driver(void)
{
	CDBG("%s called\n", __func__);
	return msm_flash_create_v4l2_subdev(&fctrl, SD_INDEX);
}

static struct msm_flash_fn_t sgm3141_func_tbl = {
	.flash_led_config = msm_camera_flash_led,
};

static struct msm_flash_ctrl_t fctrl = {
	.func_tbl = &sgm3141_func_tbl,
};

module_init(msm_flash_i2c_add_driver);
MODULE_DESCRIPTION("SGM3141 FLASH");
MODULE_LICENSE("GPL v2");
