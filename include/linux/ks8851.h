/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

/* struct ks8851_pdata - platform data definition for a KS8851 device
 * @irq_gpio - GPIO pin number for the KS8851 IRQ line
 * @rst_gpio - GPIO pin number for the KS8851 Reset line
 *
 * Platform data may be omitted (or individual GPIO numbers set to -1) to
 * avoid doing any GPIO configuration in the driver.
 */
struct ks8851_pdata {
	int irq_gpio;
	int rst_gpio;
};
