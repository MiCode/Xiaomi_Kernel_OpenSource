/*
 * arch/arm/mach-tegra/board-roth-leds.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>
#include <linux/pwm.h>
#include <mach/gpio-tegra.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>

#include "gpio-names.h"
#include "devices.h"
#include "board.h"
#include "board-common.h"
#include "board-roth.h"
#include "tegra-board-id.h"

#define LED_ENABLE_GPIO TEGRA_GPIO_PU1

#ifdef CONFIG_LEDS_PWM

static struct led_pwm roth_led_info[] = {
	{
		.name			= "roth-led",
		.default_trigger	= "none",
		.pwm_id			= 2,
		.active_low		= 0,
		.max_brightness		= 255,
		.pwm_period_ns		= 10000000,
	},
};

static struct led_pwm_platform_data roth_leds_pdata = {
	.leds		= roth_led_info,
	.num_leds	= ARRAY_SIZE(roth_led_info),
};

static struct platform_device roth_leds_pwm_device = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev	= {
		.platform_data = &roth_leds_pdata,
	},
};


#else
static struct gpio_led roth_led_info[] = {
	{
		.name			= "roth-led",
		.default_trigger	= "none",
		.gpio			= TEGRA_GPIO_PQ3,
		.active_low		= 0,
		.retain_state_suspended	= 0,
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
};

static struct gpio_led_platform_data roth_leds_pdata = {
	.leds		= roth_led_info,
	.num_leds	= ARRAY_SIZE(roth_led_info),
};

static struct platform_device roth_leds_gpio_device = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &roth_leds_pdata,
	},
};
#endif

/*static struct platform_device *roth_led_device[] = {
	&tegra_pwfm2_device,
};*/

int __init roth_led_init(void)
{
#ifdef CONFIG_LEDS_PWM
	platform_device_register(&roth_leds_pwm_device);
#else
	platform_device_register(&roth_leds_gpio_device);
#endif
	gpio_request(LED_ENABLE_GPIO, "LED Trisate Buffer OE");
	gpio_direction_output(LED_ENABLE_GPIO, 1);
	gpio_export(LED_ENABLE_GPIO, false);
	return 0;
}
