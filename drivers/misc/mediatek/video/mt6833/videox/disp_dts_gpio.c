/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "disp_dts_gpio.h"
#include "disp_helper.h"
#include "disp_drv_log.h"
#include <linux/kernel.h> /* printk */

#ifndef CONFIG_FPGA_EARLY_PORTING
static struct pinctrl *this_pctrl; /* static pinctrl instance */
#endif

/* DTS state mapping name */
static const char *this_state_name[DTS_GPIO_STATE_MAX] = {
	"lcd_bias_enp1_gpio",
	"lcd_bias_enp0_gpio",
	"lcd_bias_enn1_gpio",
	"lcd_bias_enn0_gpio",
	"lcm_rst_out1_gpio",
	"lcm_rst_out0_gpio",
	"lcm1_rst_out1_gpio",
	"lcm1_rst_out0_gpio",
	"tp_rst_out1_gpio",
	"tp_rst_out0_gpio",
	"mode_te_gpio",
	"mode_te_te",
	"mode_te1_te",
};

/* pinctrl implementation */
static long _set_state(const char *name)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	long ret = 0;
	struct pinctrl_state *pState = 0;

	if (!this_pctrl) {
		pr_info("this pctrl is null\n");
		return -1;
	}

	pState = pinctrl_lookup_state(this_pctrl, name);
	if (IS_ERR(pState)) {
		pr_info("lookup state '%s' failed\n", name);
		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(this_pctrl, pState);

exit:
	return ret; /* Good! */
#else
	return 0;
#endif
}

long disp_dts_gpio_init(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	long ret = 0;
	struct pinctrl *pctrl;

	/* retrieve */
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		DISPMSG("Cannot find disp pinctrl!");
		ret = PTR_ERR(pctrl);
		goto exit;
	}

	this_pctrl = pctrl;

exit:
	return ret;
#else
	return 0;
#endif
}

long disp_dts_gpio_select_state(enum DTS_GPIO_STATE s)
{
	if (!((unsigned int)(s) < (unsigned int)(DTS_GPIO_STATE_MAX))) {
		pr_info("GPIO STATE is invalid,state=%d\n", (unsigned int)s);
		return -1;
	}
	return _set_state(this_state_name[s]);
}
