/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

#ifndef __PMIC8058_KEYPAD_H__
#define __PMIC8058_KEYPAD_H__

#include <linux/input/matrix_keypad.h>

/*
 * NOTE: Assumption of maximum of five revisions
 * of PMIC8058 chip.
 */
#define MAX_PM8058_REVS		0x5

struct pmic8058_keypad_data {
	const struct matrix_keymap_data *keymap_data;

	const char *input_name;
	const char *input_phys_device;

	unsigned int num_cols;
	unsigned int num_rows;

	unsigned int rows_gpio_start;
	unsigned int cols_gpio_start;

	unsigned int debounce_ms[MAX_PM8058_REVS];
	unsigned int scan_delay_ms;
	unsigned int row_hold_ns;

	int keymap_size;
	const unsigned int *keymap;
	
	unsigned int wakeup;
	unsigned int rep;
};

#endif /*__PMIC8058_KEYPAD_H__ */
