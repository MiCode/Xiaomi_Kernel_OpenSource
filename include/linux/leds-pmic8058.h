/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#ifndef __LEDS_PMIC8058_H__
#define __LEDS_PMIC8058_H__

enum pmic8058_leds {
	PMIC8058_ID_LED_KB_LIGHT = 1,
	PMIC8058_ID_LED_0,
	PMIC8058_ID_LED_1,
	PMIC8058_ID_LED_2,
	PMIC8058_ID_FLASH_LED_0,
	PMIC8058_ID_FLASH_LED_1,
};

struct pmic8058_led {
	const char	*name;
	const char	*default_trigger;
	unsigned	max_brightness;
	int		id;
};

struct pmic8058_leds_platform_data {
	int	num_leds;
	struct pmic8058_led *leds;
};

int pm8058_set_flash_led_current(enum pmic8058_leds id, unsigned mA);
int pm8058_set_led_current(enum pmic8058_leds id, unsigned mA);

#endif /* __LEDS_PMIC8058_H__ */
