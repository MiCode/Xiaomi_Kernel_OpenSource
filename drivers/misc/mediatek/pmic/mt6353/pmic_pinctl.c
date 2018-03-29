/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/module.h>

#include <mt-plat/mt_devinfo.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_api_buck.h"

struct pinctrl *ppinctrl_ext_buck_vmd1;
/*-----PinCtrl START-----*/
#define EXT_BUCK_VMD1_GPIO_MODE_EN_DEFAULT 0
#define EXT_BUCK_VMD1_GPIO_MODE_EN_LOW     1
#define EXT_BUCK_VMD1_GPIO_MODE_EN_HIGH    2

char *ext_buck_vmd1_gpio_cfg[] = {"vmd1_default", "vmd1_sel_low", "vmd1_sel_high"};

void switch_ext_buck_vmd1_gpio(struct pinctrl *ppinctrl, int mode)
{
	struct pinctrl_state *ppins = NULL;

	PMICLOG("[EXT_BUCK_VMD1][PinC]%s(%d)+\n", __func__, mode);

	if (mode >= (sizeof(ext_buck_vmd1_gpio_cfg) / sizeof(ext_buck_vmd1_gpio_cfg[0]))) {
		pr_err("[EXT_BUCK_VMD1][PinC]%s(%d) fail!! - parameter error!\n", __func__, mode);
		return;
	}

	if (IS_ERR(ppinctrl)) {
		pr_err("[EXT_BUCK_VMD1][PinC]%s ppinctrl:%p is error! err:%ld\n",
		       __func__, ppinctrl, PTR_ERR(ppinctrl));
		return;
	}

	ppins = pinctrl_lookup_state(ppinctrl, ext_buck_vmd1_gpio_cfg[mode]);
	if (IS_ERR(ppins)) {
		pr_err("[EXT_BUCK_VMD1][PinC]%s pinctrl_lockup(%p, %s) fail!! ppinctrl:%p, err:%ld\n",
		       __func__, ppinctrl, ext_buck_vmd1_gpio_cfg[mode], ppins, PTR_ERR(ppins));
		return;
	}

	pinctrl_select_state(ppinctrl, ppins);
	PMICLOG("[EXT_BUCK_VMD1][PinC]%s(%d)-\n", __func__, mode);
}
/*-----PinCtrl END-----*/

void vmd1_pmic_setting_off(void)
{
	unsigned int segment = get_devinfo_with_index(21) & 0xFF;

	if (segment == 0x41 || segment == 0x45 || segment == 0x40) {/* 0x41: turbo, 0x40: eng sample, set as 0x41*/
		/* Turn OFF VCORE2 */
		/* Call PMIC driver API to configure VCORE2 OFF */
		pmic_buck_vcore2_en("VMODEM", 0, 0);
		/* Turn OFF VMD1Call PMIC driver API configure EXT_PMIC_EN as LOW (i.e. Disable RT5715) */
		pmic_set_register_value(PMIC_RG_STRUP_EXT_PMIC_SEL, 1); /* switch to SW mode */
		pmic_set_register_value(PMIC_RG_STRUP_EXT_PMIC_EN, 0); /* 1: enable, 0:disable */
		/* Wait for 100ms */
		msleep(100);
	}
}

void vmd1_pmic_setting_on(void)
{
	unsigned int segment = get_devinfo_with_index(21) & 0xFF;

	PMICLOG("before segment 0x%x\n", segment);
	if (segment == 0x41 || segment == 0x45 || segment == 0x40) {/* 0x41: turbo, 0x40: eng sample, set as 0x41*/
		/* Turn on VMD1 */
		/* 1.Call PMIC driver API to configure VMD1_SEL as L (i.e. configure VMD1 as 0.9V) */
#ifdef CONFIG_MTK_LEGACY
		/* No Need: MD1 use only.
		mt_set_gpio_mode(GPIO_VMD1_SEL_PIN, GPIO_VMD1_SEL_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_VMD1_SEL_PIN, GPIO_DIR_OUT); */
		mt_set_gpio_out(GPIO_VMD1_SEL_PIN, GPIO_OUT_ZERO);
#else
		switch_ext_buck_vmd1_gpio(ppinctrl_ext_buck_vmd1, EXT_BUCK_VMD1_GPIO_MODE_EN_LOW);
#endif
		/* 2.Call PMIC driver API configure EXT_PMIC_EN as High (i.e. Enable RT5715) */
		pmic_set_register_value(PMIC_RG_STRUP_EXT_PMIC_SEL, 1); /* switch to SW mode */
		pmic_set_register_value(PMIC_RG_STRUP_EXT_PMIC_EN, 1); /* 1: enable, 0:disable */
		/*Wait for 500us*/
		udelay(500);

		/* Turn on VCORE2 */
		/* 1.Call PMIC driver API to configure VCORE2 as HW mode */
		pmic_buck_vcore2_en("VMODEM", 0, 1);
		pmic_buck_vcore2_hw_vosel(0); /* HW source clock setting */
		/* 2.Call PMIC driver API configure VCORE2 ON voltage as 1.0V */
		pmic_set_register_value(PMIC_BUCK_VCORE2_VOSEL_ON, 0x40); /* set to 1.0V */
	} else if (segment == 0x42 || segment == 0x43 || segment == 0x46 || segment == 0x4B) {
		/* 0x42: normal 0x43: 6738*/
		/* Turn on VCORE2 */
		/* 1.Call PMIC driver API to configure VCORE2 as HW mode */
		pmic_buck_vcore2_en("VMODEM", 0, 1);
		pmic_buck_vcore2_hw_vosel(0); /* HW source clock setting */
		/* 2.Call PMIC driver API configure VCORE2 ON voltage as 1.0V */
		pmic_set_register_value(PMIC_BUCK_VCORE2_VOSEL_ON, 0x40); /* set to 1.0V */
	}
}
/*
 * ext_buck_vmd1_probe
 */
static int ext_buck_vmd1_probe(struct platform_device *dev)
{
	PMICLOG("******** ext_buck_vmd1_probe!! ********\n");

	/*-----PinCtrl START-----*/
	ppinctrl_ext_buck_vmd1 = devm_pinctrl_get(&dev->dev);
	if (IS_ERR(ppinctrl_ext_buck_vmd1)) {
		pr_err("[EXT_BUCK_VMD1][PinC]cannot find pinctrl. ptr_err:%ld\n",
		       PTR_ERR(ppinctrl_ext_buck_vmd1));
		return PTR_ERR(ppinctrl_ext_buck_vmd1);
	}
	PMICLOG("[EXT_BUCK_VMD1][PinC]devm_pinctrl_get ppinctrl:%p\n", ppinctrl_ext_buck_vmd1);

	/* Set GPIO as default */
	/* switch_ext_buck_vmd1_gpio(ppinctrl_ext_buck_vmd1, EXT_BUCK_VMD1_GPIO_MODE_EN_DEFAULT);*/
	/*-----PinCtrl END-----*/

	return 0;
}

#ifdef CONFIG_OF
/*-----PinCtrl START-----*/
static const struct of_device_id ext_buck_vmd1_of_ids[] = {
	{.compatible = "mediatek,ext_buck_vmd1"},
	{},
};
/*-----PinCtrl END-----*/
#endif

static struct platform_driver ext_buck_vmd1_driver = {
	.driver = {
		   .name = "ext_buck_vmd1",
#ifdef CONFIG_OF
		   .of_match_table = ext_buck_vmd1_of_ids,
#endif
		   },
	.probe = ext_buck_vmd1_probe,
};

static int __init ext_buck_vmd1_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&ext_buck_vmd1_driver);
	if (ret) {
		pr_err("****[ext_buck_vmd1_init] Unable to register driver (%d)\n", ret);
		return ret;
		}

	return 0;
}

static void __exit ext_buck_vmd1_exit(void)
{
	platform_driver_unregister(&ext_buck_vmd1_driver);
}
module_init(ext_buck_vmd1_init);
module_exit(ext_buck_vmd1_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EXT BUCK VMD1 Driver");
MODULE_AUTHOR("Jimmy-YJ Huang");
