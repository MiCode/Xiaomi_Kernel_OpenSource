/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LEDS_PM8XXX_H__
#define __LEDS_PM8XXX_H__


#define PM8XXX_LEDS_DEV_NAME	"pm8xxx-led"

#define PM8XXX_LED_ID_SHIFT		0
#define PM8XXX_LED_MODE_SHIFT		4
#define PM8XXX_LED_MAX_CURRENT_SHIFT	8
#define PM8XXX_LED_ID_MASK		0xF
#define PM8XXX_LED_MODE_MASK		0xF0
#define PM8XXX_LED_MAX_CURRENT_MASK	0xFFFF00

/**
 * enum pm8xxx_leds - PMIC8XXX supported led ids
 * @PM8XXX_ID_LED_KB_LIGHT - keyboard backlight led
 * @PM8XXX_ID_LED_0 - First low current led
 * @PM8XXX_ID_LED_1 - Second low current led
 * @PM8XXX_ID_LED_2 - Third low current led
 * @PM8XXX_ID_FLASH_LED_0 - First flash led
 * @PM8XXX_ID_FLASH_LED_0 - Second flash led
 */
enum pm8xxx_leds {
	PM8XXX_ID_LED_KB_LIGHT = 1,
	PM8XXX_ID_LED_0,
	PM8XXX_ID_LED_1,
	PM8XXX_ID_LED_2,
	PM8XXX_ID_FLASH_LED_0,
	PM8XXX_ID_FLASH_LED_1,
};

/**
 * pm8xxx_led_modes - Operating modes of LEDs
 */
enum pm8xxx_led_modes {
	PM8XXX_LED_MODE_MANUAL,
	PM8XXX_LED_MODE_PWM1,
	PM8XXX_LED_MODE_PWM2,
	PM8XXX_LED_MODE_PWM3,
	PM8XXX_LED_MODE_DTEST1,
	PM8XXX_LED_MODE_DTEST2,
	PM8XXX_LED_MODE_DTEST3,
	PM8XXX_LED_MODE_DTEST4
};

#endif /* __LEDS_PM8XXX_H__ */
