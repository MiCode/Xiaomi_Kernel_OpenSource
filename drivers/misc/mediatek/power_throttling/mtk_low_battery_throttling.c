// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "mtk_low_battery_throttling.h"
#include "pmic_lbat_service.h"

#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100

struct low_bat_thl_priv {
	unsigned int hv_thd_volt;
	unsigned int lv1_thd_volt;
	unsigned int lv2_thd_volt;
	int low_bat_thl_level;
	int low_bat_thl_stop;
	struct lbat_user *lbat_pt;
};

static struct low_bat_thl_priv *low_bat_thl_data;

struct low_battery_callback_table {
	void (*lbcb)(enum LOW_BATTERY_LEVEL_TAG);
};

#define LBCB_MAX_NUM 16
static struct low_battery_callback_table lbcb_tb[LBCB_MAX_NUM] = { {0} };

int register_low_battery_notify(low_battery_callback lb_cb,
				enum LOW_BATTERY_PRIO_TAG prio_val)
{
	if (prio_val >= LBCB_MAX_NUM) {
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

	if (!low_bat_thl_data)
		return;
	if (low_bat_thl_data->low_bat_thl_stop == 1) {
		pr_info("[%s] low_bat_thl_stop=%d\n",
			__func__, low_bat_thl_data->low_bat_thl_stop);
		return;
	}
	if (thd == low_bat_thl_data->hv_thd_volt)
		low_bat_thl_data->low_bat_thl_level = LOW_BATTERY_LEVEL_0;
	else if (thd == low_bat_thl_data->lv1_thd_volt)
		low_bat_thl_data->low_bat_thl_level = LOW_BATTERY_LEVEL_1;
	else if (thd == low_bat_thl_data->lv2_thd_volt)
		low_bat_thl_data->low_bat_thl_level = LOW_BATTERY_LEVEL_2;
	else {
		pr_notice("[%s] wrong threshold=%d\n", __func__, thd);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(lbcb_tb); i++) {
		if (lbcb_tb[i].lbcb)
			lbcb_tb[i].lbcb(low_bat_thl_data->low_bat_thl_level);
	}
	pr_info("[%s] low_battery_level=%d\n", __func__,
		low_bat_thl_data->low_bat_thl_level);
}

/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t low_battery_protect_ut_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "low_bat_thl_level=%d\n",
		low_bat_thl_data->low_bat_thl_level);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_level);
}

static ssize_t low_battery_protect_ut_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;
	unsigned int thd = 0;
	char cmd[21];

	dev_info(dev, "[%s]\n", __func__);

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "Utest", 5))
		return -EINVAL;

	if (val <= LOW_BATTERY_LEVEL_NUM) {
		if (val == LOW_BATTERY_LEVEL_0)
			thd = low_bat_thl_data->hv_thd_volt;
		else if (val == LOW_BATTERY_LEVEL_1)
			thd = low_bat_thl_data->lv1_thd_volt;
		else if (val == LOW_BATTERY_LEVEL_2)
			thd = low_bat_thl_data->lv2_thd_volt;
		exec_low_battery_callback(thd);
		dev_info(dev, "your input is %d(%d)\n", val, thd);
	} else {
		dev_info(dev, "wrong number (%d)\n", val);
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
	dev_dbg(dev, "low_bat_thl_stop=%d\n",
		low_bat_thl_data->low_bat_thl_stop);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_stop);
}

static ssize_t low_battery_protect_stop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	dev_info(dev, "[%s]\n", __func__);

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if ((val != 0) && (val != 1))
		val = 0;

	low_bat_thl_data->low_bat_thl_stop = val;
	dev_info(dev, "low_bat_thl_stop=%d\n",
		 low_bat_thl_data->low_bat_thl_stop);

	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_stop);

/*****************************************************************************
 * low battery protect level
 ******************************************************************************/
static ssize_t low_battery_protect_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_dbg(dev, "low_bat_thl_level=%d\n",
		low_bat_thl_data->low_bat_thl_level);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_level);
}

static ssize_t low_battery_protect_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	dev_dbg(dev, "low_bat_thl_level = %d\n",
		low_bat_thl_data->low_bat_thl_level);
	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_level);

static int low_battery_throttling_probe(struct platform_device *pdev)
{
	int ret;
	struct low_bat_thl_priv *priv;
	struct device_node *np = pdev->dev.of_node;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	low_bat_thl_data = priv;
	dev_set_drvdata(&pdev->dev, priv);

	ret = of_property_read_u32(np, "hv_thd_volt", &priv->hv_thd_volt);
	if (ret)
		priv->hv_thd_volt = POWER_INT0_VOLT;

	ret = of_property_read_u32(np, "lv1_thd_volt", &priv->lv1_thd_volt);
	if (ret)
		priv->lv1_thd_volt = POWER_INT1_VOLT;

	ret = of_property_read_u32(np, "lv2_thd_volt", &priv->lv2_thd_volt);
	if (ret)
		priv->lv2_thd_volt = POWER_INT2_VOLT;

	priv->lbat_pt = lbat_user_register("power throttling",
					   priv->hv_thd_volt,
					   priv->lv1_thd_volt,
					   priv->lv2_thd_volt,
					   exec_low_battery_callback);
	if (IS_ERR(priv->lbat_pt)) {
		ret = PTR_ERR(priv->lbat_pt);
		if (ret != -EPROBE_DEFER) {
			dev_notice(&pdev->dev,
				   "[%s] error ret=%d\n", __func__, ret);
		}
		return ret;
	}
	/* lbat_dump_reg(); */
	dev_notice(&pdev->dev, "%d mV, %d mV, %d mV Done\n",
		   priv->hv_thd_volt, priv->lv1_thd_volt, priv->lv2_thd_volt);

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

static const struct of_device_id low_bat_thl_of_match[] = {
	{ .compatible = "mediatek,low_battery_throttling", },
	{ },
};
MODULE_DEVICE_TABLE(of, low_bat_thl_of_match);

static struct platform_driver low_battery_throttling_driver = {
	.driver = {
		.name = "low_battery_throttling",
		.of_match_table = low_bat_thl_of_match,
	},
	.probe = low_battery_throttling_probe,
};
module_platform_driver(low_battery_throttling_driver);

MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK low battery throttling driver");
MODULE_LICENSE("GPL");
