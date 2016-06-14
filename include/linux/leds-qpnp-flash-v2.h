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

int qpnp_flash_led_prepare(struct led_classdev *led_cdev, int options);

#endif
