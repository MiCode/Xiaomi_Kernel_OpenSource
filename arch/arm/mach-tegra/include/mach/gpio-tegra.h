/*
 * arch/arm/mach-tegra/include/mach/gpio.h
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_TEGRA_GPIO_TEGRA_H
#define __MACH_TEGRA_GPIO_TEGRA_H

#include <linux/types.h>
#include <mach/irqs.h>

#define TEGRA_NR_GPIOS		INT_GPIO_NR

struct gpio_init_pin_info {
	char name[16];
	int gpio_nr;
	bool is_gpio;
	bool is_input;
	int value; /* Value if it is output*/
};

void tegra_gpio_init_configure(unsigned gpio, bool is_input, int value);
int tegra_gpio_get_bank_int_nr(int gpio);
int tegra_is_gpio(int);

#endif
