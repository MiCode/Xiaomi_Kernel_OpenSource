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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_led_flash.h"

static struct led_trigger *torch_trigger;

static void msm_led_torch_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	if (!torch_trigger) {
		pr_err("No torch trigger found, can't set brightness\n");
		return;
	}

	led_trigger_event(torch_trigger, value);
};

static struct led_classdev msm_torch_led = {
	.name			= "flashlight",
	.brightness_set	= msm_led_torch_brightness_set,
	.brightness		= LED_OFF,
};

int32_t msm_led_torch_create_classdev(struct platform_device *pdev,
				void *data)
{
	int rc;
	struct msm_led_flash_ctrl_t *fctrl =
		(struct msm_led_flash_ctrl_t *)data;

	if (!fctrl || !fctrl->torch_trigger) {
		pr_err("Invalid fctrl or torch trigger\n");
		return -EINVAL;
	}

	torch_trigger = fctrl->torch_trigger;
	msm_led_torch_brightness_set(&msm_torch_led, LED_OFF);

	rc = led_classdev_register(&pdev->dev, &msm_torch_led);
	if (rc) {
		pr_err("Failed to register led dev. rc = %d\n", rc);
		return rc;
	}

	return 0;
};
