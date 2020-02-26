/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __LEDS_QTI_FLASH_H
#define __LEDS_QTI_FLASH_H

#include <linux/leds.h>

#define QUERY_MAX_AVAIL_CURRENT		BIT(0)

int qpnp_flash_register_led_prepare(struct device *dev, void *data);

#if (defined CONFIG_LEDS_QTI_FLASH || defined CONFIG_LEDS_QPNP_FLASH_V2)
int qpnp_flash_led_prepare(struct led_trigger *trig, int options,
					int *max_current);
#else
static inline int qpnp_flash_led_prepare(struct led_trigger *trig, int options,
					int *max_current)
{
	return -ENODEV;
}
#endif
#endif
