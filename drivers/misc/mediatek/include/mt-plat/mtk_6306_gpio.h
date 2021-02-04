/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MTK_6306_GPIO_H_
#define _MTK_6306_GPIO_H_

enum {
	MT6306_GPIO_01 = 1,
	MT6306_GPIO_02 = 2,
	MT6306_GPIO_03 = 3,
	MT6306_GPIO_04 = 4,
	MT6306_GPIO_05 = 5,
	MT6306_GPIO_06 = 6,
	MT6306_GPIO_07 = 7,
	MT6306_GPIO_08 = 8,
	MT6306_GPIO_09 = 9,
	MT6306_GPIO_10 = 10,
	MT6306_GPIO_11 = 11,
	MT6306_GPIO_12 = 12,
	MT6306_GPIO_13 = 13,
	MT6306_GPIO_14 = 14,
	MT6306_GPIO_15 = 15,
	MT6306_GPIO_16 = 16,
	MT6306_GPIO_17 = 17,
	MT6306_GPIO_18 = 18,
};

enum {
	MT6306_GPIO_DIR_IN = 0,
	MT6306_GPIO_DIR_OUT = 1,
};

enum {
	MT6306_GPIO_OUT_LOW = 0,
	MT6306_GPIO_OUT_HIGH = 1,
};

enum {
	MT6306_GPIO_IN_LOW = 0,
	MT6306_GPIO_IN_HIGH = 1,
};

/* GPIO Driver interface  */
/* direction */
int mt6306_set_gpio_dir(unsigned long pin, unsigned long dir);
unsigned char mt6306_get_gpio_dir(unsigned long pin);

/* output */
int mt6306_set_gpio_out(unsigned long pin, unsigned long output);
unsigned char mt6306_get_gpio_out(unsigned long pin);

unsigned char mt6306_get_gpio_in(unsigned long pin);
int mt6306_set_GPIO_pin_group_power(unsigned long group, unsigned long on);


#endif
