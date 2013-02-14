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

#ifndef _ARCH_ARM_MACH_MSM_GPIO_H_
#define _ARCH_ARM_MACH_MSM_GPIO_H_

void msm_gpio_enter_sleep(int from_idle);
void msm_gpio_exit_sleep(void);

/* Locate the GPIO_OUT register for the given GPIO and return its address
 * and the bit position of the gpio's bit within the register.
 *
 * This function is used by gpiomux-v1 in order to support output transitions.
 */
void msm_gpio_find_out(const unsigned gpio, void __iomem **out,
	unsigned *offset);

#endif
