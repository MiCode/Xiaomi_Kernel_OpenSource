/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
	PM8XXX_ID_WLED,
	PM8XXX_ID_RGB_LED_RED,
	PM8XXX_ID_RGB_LED_GREEN,
	PM8XXX_ID_RGB_LED_BLUE,
	PM8XXX_ID_MAX,
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

/* current boost limit */
enum wled_current_bost_limit {
	WLED_CURR_LIMIT_105mA,
	WLED_CURR_LIMIT_385mA,
	WLED_CURR_LIMIT_525mA,
	WLED_CURR_LIMIT_805mA,
	WLED_CURR_LIMIT_980mA,
	WLED_CURR_LIMIT_1260mA,
	WLED_CURR_LIMIT_1400mA,
	WLED_CURR_LIMIT_1680mA,
};

/* over voltage protection threshold */
enum wled_ovp_threshold {
	WLED_OVP_35V,
	WLED_OVP_32V,
	WLED_OVP_29V,
	WLED_OVP_37V,
};

/**
 *  wled_config_data - wled configuration data
 *  @num_strings - number of wled strings supported
 *  @ovp_val - over voltage protection threshold
 *  @boost_curr_lim - boot current limit
 *  @cp_select - high pole capacitance
 *  @ctrl_delay_us - delay in activation of led
 *  @dig_mod_gen_en - digital module generator
 *  @cs_out_en - current sink output enable
 *  @op_fdbck - selection of output as feedback for the boost
 *  @cabc_en - enable cabc for backlight pwm control
 */
struct wled_config_data {
	u8	num_strings;
	u8	ovp_val;
	u8	boost_curr_lim;
	u8	cp_select;
	u8	ctrl_delay_us;
	bool	dig_mod_gen_en;
	bool	cs_out_en;
	bool	op_fdbck;
	bool	cabc_en;
};

/**
 * pm8xxx_led_config - led configuration parameters
 * @id - LED id
 * @mode - LED mode
 * @max_current - maximum current that LED can sustain
 * @pwm_channel - PWM channel ID the LED is driven to
 * @pwm_period_us - PWM period value in micro seconds
 * @default_state - default state of the led
 * @pwm_duty_cycles - PWM duty cycle information
 */
struct pm8xxx_led_config {
	u8	id;
	u8	mode;
	u16	max_current;
	int	pwm_channel;
	u32	pwm_period_us;
	bool	default_state;
	struct pm8xxx_pwm_duty_cycles *pwm_duty_cycles;
	struct wled_config_data	*wled_cfg;
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
