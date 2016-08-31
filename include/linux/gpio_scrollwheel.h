/*
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _GPIO_SCROLLWHEEL_H
#define _GPIO_SCROLLWHEEL_H

#define GPIO_SCROLLWHEEL_PIN_ONOFF	0
#define GPIO_SCROLLWHEEL_PIN_PRESS	1
#define GPIO_SCROLLWHEEL_PIN_ROT1	2
#define GPIO_SCROLLWHEEL_PIN_ROT2	3
#define GPIO_SCROLLWHEEL_PIN_MAX	4

struct gpio_scrollwheel_button {
	/* Configuration parameters */
	int pinaction;		/* GPIO_SCROLLWHEEL_PIN_* */
	int gpio;
	char *desc;
	int active_low;
	int debounce_interval;	/* debounce ticks interval in msecs */
};

struct gpio_scrollwheel_platform_data {
	struct gpio_scrollwheel_button *buttons;
	int nbuttons;
	unsigned int rep:1;	/* enable input subsystem auto repeat */
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
};

#endif

