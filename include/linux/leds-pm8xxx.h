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

#include <linux/kernel.h>
#include <linux/mfd/pm8xxx/pwm.h>

#define PM8XXX_LEDS_DEV_NAME	"pm8xxx-led"

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

/**
 * pm8xxx_led_config - led configuration parameters
 * @id - LED id
 * @mode - LED mode
 * @max_current - maximum current that LED can sustain
 * @pwm_channel - PWM channel ID the LED is driven to
 * @pwm_period_us - PWM period value in micro seconds
 * @pwm_duty_cycles - PWM duty cycle information
 */
struct pm8xxx_led_config {
	u8	id;
	u8	mode;
	u16	max_current;
	int	pwm_channel;
	u32	pwm_period_us;
	struct pm8xxx_pwm_duty_cycles *pwm_duty_cycles;
};

/**
 * pm8xxx_led_platform_data - platform data for LED
 * @led_core - array of LEDs. Each datum in array contains
 *	core data for the LED
 * @configs - array of platform configuration parameters
 *	for each LED. It maps one-to-one with
 *	array of LEDs
 * @num_configs - count of members of configs array
 */
struct pm8xxx_led_platform_data {
	struct	led_platform_data	*led_core;
	struct	pm8xxx_led_config	*configs;
	u32				num_configs;
};
#endif /* __LEDS_PM8XXX_H__ */
