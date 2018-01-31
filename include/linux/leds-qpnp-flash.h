/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LEDS_QPNP_FLASH_H
#define __LEDS_QPNP_FLASH_H

#include <linux/leds.h>

#define ENABLE_REGULATOR	BIT(0)
#define DISABLE_REGULATOR	BIT(1)
#define QUERY_MAX_CURRENT	BIT(2)

#define FLASH_LED_PREPARE_OPTIONS_MASK	GENMASK(3, 0)

#if (defined CONFIG_LEDS_QPNP_FLASH || defined CONFIG_LEDS_QPNP_FLASH_V2)
extern int (*qpnp_flash_led_prepare)(struct led_trigger *trig, int options,
					int *max_current);
#else
static inline int qpnp_flash_led_prepare(struct led_trigger *trig, int options,
					int *max_current)
{
	return -ENODEV;
}
#endif
#endif
