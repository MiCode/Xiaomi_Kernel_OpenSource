// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include "mtk_low_battery_throttling.h"
#include "pmic_lbat_service.h"

#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100

static struct lbat_user *lbat_pt;
static int g_low_battery_level;

struct low_battery_callback_table {
	void (*lbcb)(enum LOW_BATTERY_LEVEL_TAG);
};

#define LBCB_MAX_NUM 16
static struct low_battery_callback_table lbcb_tb[LBCB_MAX_NUM] = { {0} };

int register_low_battery_notify(low_battery_callback lb_cb,
				enum LOW_BATTERY_PRIO_TAG prio_val)
{
	if (prio_val >= LBCB_MAX_NUM || prio_val < 0) {
		pr_notice("[%s] prio_val=%d, out of boundary\n",
			__func__, prio_val);
		return -EINVAL;
	}
	lbcb_tb[prio_val].lbcb = lb_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);
	return 0;
}
EXPORT_SYMBOL(register_low_battery_notify);

void exec_low_battery_callback(unsigned int thd)
{
	int i = 0;

	switch (thd) {
	case POWER_INT0_VOLT:
		g_low_battery_level = LOW_BATTERY_LEVEL_0;
		break;
	case POWER_INT1_VOLT:
		g_low_battery_level = LOW_BATTERY_LEVEL_1;
		break;
	case POWER_INT2_VOLT:
		g_low_battery_level = LOW_BATTERY_LEVEL_2;
		break;
	default:
		pr_notice("[%s] wrong threshold=%d\n", __func__, thd);
		return;
	}
	for (i = 0; i < ARRAY_SIZE(lbcb_tb); i++) {
		if (lbcb_tb[i].lbcb)
			lbcb_tb[i].lbcb(g_low_battery_level);
	}
	pr_info("[%s] low_battery_level=%d\n", __func__, g_low_battery_level);

}

static int low_battery_throttling_probe(struct platform_device *pdev)
{
	int ret = 0;

	lbat_pt = lbat_user_register("power throttling", POWER_INT0_VOLT,
				     POWER_INT1_VOLT, POWER_INT2_VOLT,
				     exec_low_battery_callback);
	if (IS_ERR(lbat_pt)) {
		ret = PTR_ERR(lbat_pt);
		if (ret != -EPROBE_DEFER) {
			dev_notice(&pdev->dev,
				   "[%s] error ret=%d\n", __func__, ret);
		}
		return ret;
	}

	/* lbat_dump_reg(); */
	dev_notice(&pdev->dev, "%d mV, %d mV, %d mV Done\n",
		   POWER_INT0_VOLT, POWER_INT1_VOLT, POWER_INT2_VOLT);
	return ret;
}

static struct platform_device low_battery_throttling_device = {
	.name = "low_battery_throttling",
	.id = PLATFORM_DEVID_NONE,
};

static struct platform_driver low_battery_throttling_driver = {
	.driver = {
		.name = "low_battery_throttling",
	},
	.probe = low_battery_throttling_probe,
};

static int __init low_battery_throttling_init(void)
{
	int ret;

	ret = platform_device_register(&low_battery_throttling_device);
	if (ret) {
		pr_notice("[%s] device register fail ret %d\n", __func__, ret);
		return ret;
	}
	ret = platform_driver_register(&low_battery_throttling_driver);
	if (ret) {
		pr_notice("[%s] driver register fail ret %d\n", __func__, ret);
		return ret;
	}
	return 0;
}
module_init(low_battery_throttling_init);

MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK low battery throttling driver");
MODULE_LICENSE("GPL");
