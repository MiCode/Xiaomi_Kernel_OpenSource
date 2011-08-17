/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __MFD_TABLA_PDATA_H__

#define __MFD_TABLA_PDATA_H__

#include <linux/slimbus/slimbus.h>

struct tabla_pdata {
	int irq;
	int irq_base;
	int num_irqs;
	int reset_gpio;
	struct slim_device slimbus_slave_device;
};

#endif
