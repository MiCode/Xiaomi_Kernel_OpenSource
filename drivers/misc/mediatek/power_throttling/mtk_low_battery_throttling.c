// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>

#include "mtk_low_battery_throttling.h"
#include "pmic_lbat_service.h"

#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100

static struct lbat_user *lbat_pt;
static int g_low_battery_level;
static int g_low_battery_stop;

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

	if (g_low_battery_stop == 1) {
		pr_info("[%s] g_low_battery_stop=%d\n"
			, __func__, g_low_battery_stop);
	} else {
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
		pr_info("[%s] low_battery_level=%d\n", __func__,
			g_low_battery_level);
	}
}

/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t low_battery_protect_ut_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	pr_debug("[%s] g_low_battery_level=%d\n", __func__,
		g_low_battery_level);
	return sprintf(buf, "%u\n", g_low_battery_level);
}

static ssize_t low_battery_protect_ut_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;
	unsigned int thd;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n",
			__func__, buf, size);
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if (val <= 2) {
			if (val == LOW_BATTERY_LEVEL_0)
				thd = POWER_INT0_VOLT;
			else if (val == LOW_BATTERY_LEVEL_1)
				thd = POWER_INT1_VOLT;
			else if (val == LOW_BATTERY_LEVEL_2)
				thd = POWER_INT2_VOLT;
			exec_low_battery_callback(thd);
			pr_info("[%s] your input is %d(%d)\n",
				__func__, val, thd);
		} else {
			pr_info("[%s] wrong number (%d)\n", __func__, val);
		}
	}
	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_ut);

/*****************************************************************************
 * low battery protect stop
 ******************************************************************************/
static ssize_t low_battery_protect_stop_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	pr_debug("[%s] g_low_battery_stop=%d\n", __func__, g_low_battery_stop);
	return sprintf(buf, "%u\n", g_low_battery_stop);
}

static ssize_t low_battery_protect_stop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n",
			__func__, buf, size);
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_low_battery_stop = val;
		pr_info("[%s] g_low_battery_stop=%d\n",
			__func__, g_low_battery_stop);
	}
	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_stop);

/*****************************************************************************
 * low battery protect level
 ******************************************************************************/
static ssize_t low_battery_protect_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_debug("[%s] g_low_battery_level=%d\n",
		__func__, g_low_battery_level);
	return sprintf(buf, "%u\n", g_low_battery_level);
}

static ssize_t low_battery_protect_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	pr_debug("[%s] g_low_battery_level = %d\n", __func__,
		g_low_battery_level);

	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_level);

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

	ret = device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_ut);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_stop);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_level);
	if (ret)
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);

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
