/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef __MFD_CDC_GPIO_CTRL_H_
#define __MFD_CDC_GPIO_CTRL_H_

#include <linux/types.h>
#include <linux/of.h>

#ifdef CONFIG_WCD9335_CODEC
extern int wcd_gpio_ctrl_select_sleep_state(struct device_node *);
extern int wcd_gpio_ctrl_select_active_state(struct device_node *);

#else
int wcd_gpio_ctrl_select_sleep_state(struct device_node *np)
{
	return 0;
}
int wcd_gpio_ctrl_select_active_state(struct device_node *np)
{
	return 0;
}
#endif

#endif
