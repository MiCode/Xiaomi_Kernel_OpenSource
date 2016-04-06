/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.	1
 */

#ifndef __LEDS_QPNP_FLASH_V2_H
#define __LEDS_QPNP_FLASH_V2_H

#include <linux/leds.h>
#include "leds.h"

#define ENABLE_REGULATOR	BIT(0)
#define QUERY_MAX_CURRENT	BIT(1)

/*
 * Configurations for each individual LED
 */
struct flash_node_data {
	struct platform_device		*pdev;
	struct led_classdev		cdev;
	struct pinctrl			*pinctrl;
	struct pinctrl_state		*gpio_state_active;
	struct pinctrl_state		*gpio_state_suspend;
	struct pinctrl_state		*hw_strobe_state_active;
	struct pinctrl_state		*hw_strobe_state_suspend;
	int				hw_strobe_gpio;
	int				ires_ua;
	int				max_current;
	int				current_ma;
	u8				duration;
	u8				id;
	u8				type;
	u8				ires;
	u8				hdrm_val;
	u8				current_reg_val;
	u8				trigger;
	bool				led_on;
};

struct flash_switch_data {
	struct platform_device		*pdev;
	struct led_classdev		cdev;
};

int qpnp_flash_led_prepare(struct led_classdev *led_cdev, int options);

#endif
