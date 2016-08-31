/*
 * arch/arm/mach-tegra/board-loki-fan.c
 *
 * Copyright (c) 2013 NVIDIA CORPORATION, All rights reserved.
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
#include "board.h"
#include "board-common.h"
#include "board-loki.h"
#include "tegra-board-id.h"

static int active_pwm_loki[MAX_ACTIVE_STATES] = {
		0, 80*1024, 110*1024 , 150*1024, 200*1024, 240*1024,
		245*1024, 250*1024, 252*1024, 255*1024};

static int active_pwm_foster[MAX_ACTIVE_STATES] = {
		0, 70*1024, 115*1024 , 135*1024, 135*1024, 240*1024,
		245*1024, 250*1024, 252*1024, 255*1024};

static struct pwm_fan_platform_data fan_data_delta_6k = {
	.active_steps = MAX_ACTIVE_STATES,
	.active_rpm = {
		0, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 10000, 11000},
	.active_rru = {1024*40, 1024*2, 1024, 256,
						256, 256, 256, 256, 256, 256},
	.active_rrd = {1024*40, 1024*2, 1024, 256, 256,
						256, 256, 128, 128, 128},
	.state_cap_lookup = {2, 2, 2, 2, 2, 2, 2, 3, 3, 3},
	.pwm_period = 256,
	.pwm_id = 0,
	.step_time = 100, /*msecs*/
	.state_cap = 7,
	.precision_multiplier = 1024,
	.tach_gpio = TEGRA_GPIO_PU2,
	.pwm_gpio = TEGRA_GPIO_PU3,
};

static struct platform_device pwm_fan_therm_cooling_device_delta_6k = {
	.name = "pwm-fan",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &fan_data_delta_6k,
	},
};

int __init loki_fan_init(void)
{
	int err;
	struct board_info board_info;

	tegra_get_board_info(&board_info);
	if ((board_info.sku == 900) && (board_info.board_id == BOARD_P2530)) {
		memcpy((&fan_data_delta_6k)->active_pwm, &active_pwm_foster,
		sizeof(active_pwm_foster));
	} else {
		memcpy((&fan_data_delta_6k)->active_pwm, &active_pwm_loki,
		sizeof(active_pwm_loki));
	}

	err = gpio_request(TEGRA_GPIO_PU3, "pwm-fan");
	if (err < 0) {
		pr_err("FAN:gpio request failed\n");
		return err;
	}

	platform_device_register(
		&pwm_fan_therm_cooling_device_delta_6k);
			pr_info("Registering Fan!\n");
	return 0;
}
