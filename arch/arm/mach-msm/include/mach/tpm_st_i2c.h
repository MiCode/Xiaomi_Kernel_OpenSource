/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#ifndef _TPM_ST_I2C_H_
#define _TPM_ST_I2C_H_

struct tpm_st_i2c_platform_data {
	int accept_cmd_gpio;
	int data_avail_gpio;
	int accept_cmd_irq;
	int data_avail_irq;
	int (*gpio_setup)(void);
	void (*gpio_release)(void);
};

#endif
