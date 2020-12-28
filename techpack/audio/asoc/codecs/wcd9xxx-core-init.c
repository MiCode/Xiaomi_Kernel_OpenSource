// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <asoc/msm-cdc-pinctrl.h>
#include <asoc/wcd9xxx-irq.h>
#include <asoc/core.h>

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
