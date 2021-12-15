/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#ifdef CONFIG_MTK_PMIC_NEW_ARCH
#include <mt-plat/upmu_common.h>
#endif
#include "vibrator.h"

#define T    "vibrator"
struct vibrator_hw *pvib_cust;

#define OC_INTR_INIT_DELAY      (3)

void vibr_Enable_HW(void)
{
#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
	pmic_set_register_value(PMIC_RG_LDO_VIBR_EN, 1);
#endif
	mdelay(OC_INTR_INIT_DELAY);
	pmic_enable_interrupt(INT_VIBR_OC, 1, "vibr");
}

void vibr_Disable_HW(void)
{
	pmic_enable_interrupt(INT_VIBR_OC, 0, "vibr");
#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
	pmic_set_register_value(PMIC_RG_LDO_VIBR_EN, 0);
#endif
}

void init_vibr_oc_handler(void (*vibr_oc_func)(void))
{
	pmic_register_interrupt_callback(INT_VIBR_OC, vibr_oc_func);
}

/******************************************
 * Set RG_VIBR_VOSEL	Output voltage select
 *  hw->vib_vol:  Voltage selection
 * 4'b0000 :1.2V
 * 4'b0001 :1.3V
 * 4'b0010 :1.5V
 * 4'b0100 :1.8V
 * 4'b0101 :2.0V
 * 4'b1001 :2.8V
 * 4'b1011 :3.0V
 * 4'b1101 :3.3V
 *******************************************/
void init_cust_vibrator_dtsi(struct platform_device *pdev)
{
	int ret;

	if (pvib_cust == NULL) {
		pvib_cust = kmalloc(sizeof(struct vibrator_hw), GFP_KERNEL);
		if (pvib_cust == NULL) {
			pr_debug(T "%s kmalloc fail\n", __func__);
			return;
		}
		ret = of_property_read_u32(pdev->dev.of_node, "vib_timer",
			&(pvib_cust->vib_timer));
		if (!ret)
			pr_debug(T "vib_timer:%d\n", pvib_cust->vib_timer);
		else
			pvib_cust->vib_timer = 25;
#ifdef CUST_VIBR_LIMIT
		ret = of_property_read_u32(pdev->dev.of_node, "vib_limit",
			&(pvib_cust->vib_limit));
		if (!ret)
			pr_debug(T "vib_limit : %d\n", pvib_cust->vib_limit);
		else
			pvib_cust->vib_limit = 9;
#endif

#ifdef CUST_VIBR_VOL
		ret = of_property_read_u32(pdev->dev.of_node, "vib_vol",
			&(pvib_cust->vib_vol));
		if (!ret)
			pr_debug(T "vib_vol: %d\n", pvib_cust->vib_vol);
		else
			pvib_cust->vib_vol = 0x05;
#endif
		pr_debug(T "pvib_cust = %d, %d, %d\n",
			pvib_cust->vib_timer, pvib_cust->vib_limit,
					pvib_cust->vib_vol);
	}
}

struct vibrator_hw *get_cust_vibrator_dtsi(void)
{
	if (pvib_cust == NULL)
		pr_debug(T "get_cust_vibrator_dtsi fail, pvib_cust is NULL\n");
	return pvib_cust;
}

void vibr_power_set(void)
{
#ifdef CUST_VIBR_VOL
	struct vibrator_hw *hw = get_cust_vibrator_dtsi();

	if (hw != NULL) {
		pr_debug(T "vibr_init: set voltage = %d\n", hw->vib_vol);
#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
		pmic_set_register_value(PMIC_RG_VIBR_VOSEL, hw->vib_vol);
#endif
	} else {
		pr_debug("vibr_init: can not get  dtsi!\n");
	}
#endif
}

struct vibrator_hw *mt_get_cust_vibrator_hw(void)
{
	struct vibrator_hw *hw = get_cust_vibrator_dtsi();
	return hw;
}
