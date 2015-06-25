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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_led_flash.h"

extern int set_led_status(void *data, u64 val);
static struct msm_led_flash_ctrl_t *g_fctrl = NULL;

static void msm_led_torch_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	if(value > 0)
		value = 128 - value;
	set_led_status(g_fctrl, value);
};

static struct led_classdev msm_torch_led = {
	.name			= "flashlight",
	.brightness_set	= msm_led_torch_brightness_set,
	.brightness		= LED_OFF,
};

int32_t msm_led_flashlight_create_classdev(struct platform_device *pdev,
				void *data)
{
	int32_t i, rc = 0;
	struct msm_led_flash_ctrl_t *fctrl =
		(struct msm_led_flash_ctrl_t *)data;

	if (!fctrl) {
		pr_err("Invalid fctrl\n");
		return -EINVAL;
	}

	g_fctrl = fctrl;

	rc = led_classdev_register(&pdev->dev, &msm_torch_led);
        if (rc) {
	        pr_err("Failed to register %d led dev. rc = %d\n", i, rc);
                return rc;
        }

	return 0;
};
