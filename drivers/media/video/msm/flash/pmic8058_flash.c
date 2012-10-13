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

#define SD_INDEX 0

static struct msm_flash_ctrl_t fctrl;

static int msm_camera_pmic_flash(struct msm_flash_ctrl_t *fctrl,
	uint8_t led_state)
{
	int rc = 0;
	struct msm_camera_sensor_flash_pmic *pmic =
		&fctrl->flash_data->flash_src->_fsrc.pmic_src;

	switch (led_state) {
	case MSM_CAMERA_LED_OFF:
		rc = pmic->pmic_set_current(pmic->led_src_1, 0);
		if (pmic->num_of_src > 1)
			rc = pmic->pmic_set_current(pmic->led_src_2, 0);
		break;

	case MSM_CAMERA_LED_LOW:
		rc = pmic->pmic_set_current(pmic->led_src_1,
				pmic->low_current);
		if (pmic->num_of_src > 1)
			rc = pmic->pmic_set_current(pmic->led_src_2, 0);
		break;

	case MSM_CAMERA_LED_HIGH:
		rc = pmic->pmic_set_current(pmic->led_src_1,
			pmic->high_current);
		if (pmic->num_of_src > 1)
			rc = pmic->pmic_set_current(pmic->led_src_2,
				pmic->high_current);
		break;

	case MSM_CAMERA_LED_INIT:
	case MSM_CAMERA_LED_RELEASE:
		 break;

	default:
		rc = -EFAULT;
		break;
	}
	CDBG("flash_set_led_state: return %d\n", rc);

	return rc;
}

static int __init msm_flash_i2c_add_driver(void)
{
	CDBG("%s called\n", __func__);
	return msm_flash_create_v4l2_subdev(&fctrl, SD_INDEX);
}

static struct msm_flash_fn_t pmic_flash_func_tbl = {
	.flash_led_config = msm_camera_pmic_flash,
};

static struct msm_flash_ctrl_t fctrl = {
	.func_tbl = &pmic_flash_func_tbl,
};

module_init(msm_flash_i2c_add_driver);
MODULE_DESCRIPTION("PMIC FLASH");
MODULE_LICENSE("GPL v2");
