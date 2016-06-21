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

#ifndef __MFD_CDC_PINCTRL_H_
#define __MFD_CDC_PINCTRL_H_

#include <linux/types.h>
#include <linux/of.h>

#ifdef CONFIG_MSM_CDC_PINCTRL
extern int msm_cdc_pinctrl_select_sleep_state(struct device_node *);
extern int msm_cdc_pinctrl_select_active_state(struct device_node *);
extern bool msm_cdc_pinctrl_get_state(struct device_node *);
extern int msm_cdc_get_gpio_state(struct device_node *);

#else
int msm_cdc_pinctrl_select_sleep_state(struct device_node *np)
{
	return 0;
}
int msm_cdc_pinctrl_select_active_state(struct device_node *np)
{
	return 0;
}
int msm_cdc_get_gpio_state(struct device_node *np)
{
	return 0;
}
#
#endif

#endif
