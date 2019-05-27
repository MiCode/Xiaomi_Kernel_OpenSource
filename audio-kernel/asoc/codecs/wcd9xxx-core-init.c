/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include "msm-cdc-pinctrl.h"
#include "wcd9xxx-irq.h"
#include "core.h"

#define NUM_DRIVERS_REG_RET 3

static int __init wcd9xxx_core_init(void)
{
	int ret[NUM_DRIVERS_REG_RET] = {0};
	int i = 0;

	ret[0] = msm_cdc_pinctrl_drv_init();
	if (ret[0])
		pr_err("%s: Failed init pinctrl drv: %d\n", __func__, ret[0]);

	ret[1] = wcd9xxx_irq_drv_init();
	if (ret[1])
		pr_err("%s: Failed init irq drv: %d\n", __func__, ret[1]);

	ret[2] = wcd9xxx_init();
	if (ret[2])
		pr_err("%s: Failed wcd core drv: %d\n", __func__, ret[2]);

	for (i = 0; i < NUM_DRIVERS_REG_RET; i++) {
		if (ret[i])
			return ret[i];
	}

	return 0;
}
module_init(wcd9xxx_core_init);

static void __exit wcd9xxx_core_exit(void)
{
	wcd9xxx_exit();
	wcd9xxx_irq_drv_exit();
	msm_cdc_pinctrl_drv_exit();
}
module_exit(wcd9xxx_core_exit);

MODULE_DESCRIPTION("WCD9XXX CODEC core init driver");
MODULE_LICENSE("GPL v2");
