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

/*
 * Configurations for each individual LED
 */
struct flash_node_data {
	struct platform_device		*pdev;
	struct led_classdev		cdev;
	struct pinctrl			*pinctrl;
	struct pinctrl_state		*gpio_state_active;
	struct pinctrl_state		*gpio_state_suspend;
	int				ires_ua;
	u16				prgm_current;
	u8				duration;
	u8				id;
	u8				type;
	u8				ires;
	u8				hdrm_val;
	u8				brightness;
	bool				led_on;
};

struct flash_switch_data {
	struct platform_device		*pdev;
	struct led_classdev		cdev;
};

#endif
