/*
 * arch/arm/mach-tegra/board-roth-fan.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/platform_data/pwm_fan.h>
#include <linux/platform_device.h>

#include <mach/gpio-tegra.h>

#include "gpio-names.h"
#include "devices.h"
#include "board-roth.h"

static struct pwm_fan_platform_data fan_data = {
	.active_steps = MAX_ACTIVE_STATES,
	.active_temps = {59, 60, 61, 62, 63, 64, 65, 66, 67, 71},
	.active_rpm = {
		0, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 11000},
	.active_pwm = {0, 158*1024, 227*1024 , 230*1024, 235*1024, 240*1024,
				245*1024, 250*1024, 252*1024, 255*1024},
	.active_rru = {1024*50, 1024, 1024, 256, 256, 256, 256, 256, 256, 256},
	.active_rrd = {1024*50, 1024, 1024, 256, 256, 256, 256, 128, 128, 128},
	.pwm_period = 256,
	.pwm_id = 0,
	.step_time = 100, /*msecs*/
	.pwm_cap = 158,
	.precision_multiplier = 1024,
};

static struct platform_device pwm_fan_therm_cooling_device = {
	.name = "pwm-fan",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &fan_data,
	},
};

static struct platform_device *roth_fan_device[] = {
	&tegra_pwfm0_device,
};

int __init roth_fan_init(void)
{
	int err;

	err = gpio_request(TEGRA_GPIO_PU3, "pwm-fan");
	if (err < 0) {
		pr_err("FAN:gpio request failed\n");
		return err;
	}
	gpio_free(TEGRA_GPIO_PU3);
	platform_device_register(&pwm_fan_therm_cooling_device);

	err = platform_add_devices(roth_fan_device,
				ARRAY_SIZE(roth_fan_device));
	if (err < 0) {
		pr_err("FAN:pwm fan device device registration failed\n");
		return err;
	}
	return 0;
}
