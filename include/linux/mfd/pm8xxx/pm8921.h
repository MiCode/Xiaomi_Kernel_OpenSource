/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm PMIC 8921 driver header file
 *
 */

#ifndef __MFD_PM8921_H
#define __MFD_PM8921_H

#include <linux/mfd/pm8xxx/irq.h>
#include <linux/mfd/pm8xxx/gpio.h>

#define PM8921_NR_IRQS		256

#define PM8921_NR_GPIOS		44

#define PM8921_GPIO_BLOCK_START	24
#define PM8921_IRQ_BLOCK_BIT(block, bit) ((block) * 8 + (bit))

/* GPIOs [1,N] */
#define PM8921_GPIO_IRQ(base, gpio)	((base) + \
		PM8921_IRQ_BLOCK_BIT(PM8921_GPIO_BLOCK_START, (gpio)-1))

struct pm8921_platform_data {
	int					irq_base;
	struct pm8xxx_irq_platform_data		*irq_pdata;
	struct pm8xxx_gpio_platform_data	*gpio_pdata;
};

#endif
