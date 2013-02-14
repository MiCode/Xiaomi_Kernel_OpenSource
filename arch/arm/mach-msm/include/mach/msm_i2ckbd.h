/* Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_I2CKBD_H_
#define _MSM_I2CKBD_H_

struct msm_i2ckbd_platform_data {
	uint8_t hwrepeat;
	uint8_t scanset1;
	int  gpioreset;
	int  gpioirq;
	int  (*gpio_setup) (void);
	void (*gpio_shutdown)(void);
	void (*hw_reset) (int);
};

#endif
