/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MFD_CDC_PINCTRL_H_
#define __MFD_CDC_PINCTRL_H_

#include <linux/types.h>
#include <linux/of.h>

#if IS_ENABLED(CONFIG_MSM_CDC_PINCTRL)
extern int msm_cdc_pinctrl_select_sleep_state(struct device_node *np);
extern int msm_cdc_pinctrl_select_active_state(struct device_node *np);
extern int msm_cdc_pinctrl_select_alt_active_state(struct device_node *np);
extern int msm_cdc_pinctrl_get_state(struct device_node *np);
extern int msm_cdc_get_gpio_state(struct device_node *np);
extern int msm_cdc_pinctrl_set_wakeup_capable(struct device_node *np,
					      bool enable);
int msm_cdc_pinctrl_drv_init(void);
void msm_cdc_pinctrl_drv_exit(void);

#else
int msm_cdc_pinctrl_select_sleep_state(struct device_node *np)
{
	return 0;
}
int msm_cdc_pinctrl_select_active_state(struct device_node *np)
{
	return 0;
}
int msm_cdc_pinctrl_select_alt_active_state(struct device_node *np)
{
	return 0;
}
int msm_cdc_get_gpio_state(struct device_node *np)
{
	return 0;
}
int msm_cdc_pinctrl_drv_init(void)
{
	return 0;
}
void msm_cdc_pinctrl_drv_exit(void)
{
}
int msm_cdc_pinctrl_get_state(struct device_node *np)
{
	return true;
}
static int msm_cdc_pinctrl_set_wakeup_capable(struct device_node *np,
					      bool enable)
{
	return 0;
}
#endif

#endif
