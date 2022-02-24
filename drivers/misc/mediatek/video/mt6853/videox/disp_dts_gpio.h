// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __DISP_DTS_GPIO_H__
#define __DISP_DTS_GPIO_H__


#include <linux/platform_device.h>	/* struct platform_device */

/* DTS state */
enum DTS_GPIO_STATE {
	DTS_GPIO_STATE_LCD_BIAS_ENP1 = 0,
	DTS_GPIO_STATE_LCD_BIAS_ENP0,
	DTS_GPIO_STATE_LCD_BIAS_ENN1,
	DTS_GPIO_STATE_LCD_BIAS_ENN0,
	DTS_GPIO_STATE_LCM_RST_OUT1,
	DTS_GPIO_STATE_LCM_RST_OUT0,
	DTS_GPIO_STATE_LCM1_RST_OUT1,
	DTS_GPIO_STATE_LCM1_RST_OUT0,
	DTS_GPIO_STATE_TP_RST_OUT1,
	DTS_GPIO_STATE_TP_RST_OUT0,
	DTS_GPIO_STATE_TE_MODE_GPIO,	/* mode_te_gpio */
	DTS_GPIO_STATE_TE_MODE_TE,	/* mode_te_te */
	DTS_GPIO_STATE_TE1_MODE_TE,	/* mode_te1_te */
	DTS_GPIO_STATE_MAX,		/* for array size */
};

/* this function MUST be called in mtkfb_probe.
 *  @param *pdev    - reference of struct platform_device which contains pinctrl
 *                    state information of GPIO
 *  @return         - 0 for OK, otherwise returns PTR_ERR(pdev).
 */
long disp_dts_gpio_init(struct platform_device *pdev);

/* set gpio according sepcified DTS state.
 *  @notice         - to call this function, you MUST init this module first.
 *                    If not, we will trigger BUG_ON(0).
 *  @param s        - state which describes GPIO statement.
 *  @return         - 0 for OK, otherwise returns PTR_ERR(pdev).
 */
long disp_dts_gpio_select_state(enum DTS_GPIO_STATE s);

/* repo of initialization */
#ifdef CONFIG_MTK_LEGACY
#define disp_dts_gpio_init_repo(x)  (0)
#else
#define disp_dts_gpio_init_repo(x)  (disp_dts_gpio_init(x))
#endif

#endif/*__DISP_DTS_GPIO_H__ */
