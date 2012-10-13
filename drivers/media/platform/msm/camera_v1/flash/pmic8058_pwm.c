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
#include <linux/pwm.h>

#include "msm_flash.h"
#define SD_INDEX 0

static struct msm_flash_ctrl_t fctrl;

static int msm_camera_flash_pwm(struct msm_flash_ctrl_t *fctrl,
	uint8_t led_state)
{
	int rc = 0;
	struct msm_camera_sensor_flash_pwm *pwm =
		&fctrl->flash_data->flash_src->_fsrc.pwm_src;
	int PWM_PERIOD = USEC_PER_SEC / pwm->freq;

	struct pwm_device *flash_pwm = (struct pwm_device *)fctrl->data;

	if (!flash_pwm) {
		flash_pwm = pwm_request(pwm->channel, "camera-flash");
		if (flash_pwm == NULL || IS_ERR(flash_pwm)) {
			pr_err("%s: FAIL pwm_request(): flash_pwm=%p\n",
			       __func__, flash_pwm);
			flash_pwm = NULL;
			return -ENXIO;
		}
	}

	switch (led_state) {
	case MSM_CAMERA_LED_LOW:
		rc = pwm_config(flash_pwm,
			(PWM_PERIOD/pwm->max_load)*pwm->low_load,
			PWM_PERIOD);
		if (rc >= 0)
			rc = pwm_enable(flash_pwm);
		break;

	case MSM_CAMERA_LED_HIGH:
		rc = pwm_config(flash_pwm,
			(PWM_PERIOD/pwm->max_load)*pwm->high_load,
			PWM_PERIOD);
		if (rc >= 0)
			rc = pwm_enable(flash_pwm);
		break;

	case MSM_CAMERA_LED_OFF:
		pwm_disable(flash_pwm);
		break;
	case MSM_CAMERA_LED_INIT:
	case MSM_CAMERA_LED_RELEASE:
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

static struct msm_flash_fn_t pmic8058_pwm_func_tbl = {
	.flash_led_config = msm_camera_flash_pwm,
};

static struct msm_flash_ctrl_t fctrl = {
	.func_tbl = &pmic8058_pwm_func_tbl,
};

module_init(msm_flash_i2c_add_driver);
MODULE_DESCRIPTION("PMIC FLASH");
MODULE_LICENSE("GPL v2");
