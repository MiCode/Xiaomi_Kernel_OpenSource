// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include "mtk_bp_thl.h"

#define BAT_PERCENT_LIMIT 15
#define BAT_PERCENT_LIMIT_EXT 15
#define BAT_PERCENT_LIMIT_RELEASE_EXT 15

static struct task_struct *bp_notify_thread;
static bool bp_notify_flag;
static bool bp_notify_flag_ext;

static DECLARE_WAIT_QUEUE_HEAD(bp_notify_waiter);
static struct wakeup_source *bp_notify_lock;

struct bp_thl_callback_table {
	void (*bpcb)(enum BATTERY_PERCENT_LEVEL_TAG);
};

#define BPCB_MAX_NUM 16

static struct bp_thl_callback_table bpcb_tb[BPCB_MAX_NUM] = { {0} };
static struct bp_thl_callback_table bpcb_tb_ext[BPCB_MAX_NUM] = { {0} };


static struct notifier_block bp_nb;

struct bp_thl_priv {
	int soc_limit;
	int soc_limit_release;
	int soc_limit_ext;
	int soc_limit_ext_release;
	int bp_thl_lv;
	int bp_thl_lv_ext;
	int bp_thl_stop;
};

static struct bp_thl_priv *bp_thl_data;

void register_bp_thl_notify(
	battery_percent_callback bp_cb,
	BATTERY_PERCENT_PRIO prio_val)
{
	if (!bp_thl_data) {
		pr_info("[%s] bp_thl not init\n", __func__);
		return;
	}

	if (prio_val >= BPCB_MAX_NUM) {
		pr_info("[%s] prio_val=%d, out of boundary\n", __func__, prio_val);
		return;
	}

	bpcb_tb[prio_val].bpcb = bp_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (bp_thl_data->bp_thl_lv == 1) {
		pr_info("[%s] level 1 happen\n", __func__);
		if (bp_cb != NULL)
			bp_cb(BATTERY_PERCENT_LEVEL_1);
	}
}
EXPORT_SYMBOL(register_bp_thl_notify);

void register_bp_thl_notify_ext(
	battery_percent_callback bp_cb,
	BATTERY_PERCENT_PRIO prio_val)
{
	if (!bp_thl_data) {
		pr_info("[%s] bp_thl not init\n", __func__);
		return;
	}

	if (prio_val >= BPCB_MAX_NUM) {
		pr_info("[%s] prio_val=%d, out of boundary\n", __func__, prio_val);
		return;
	}

	bpcb_tb_ext[prio_val].bpcb = bp_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (bp_thl_data->bp_thl_lv_ext == 1) {
		pr_info("[%s] level 1 happen\n", __func__);
		if (bp_cb != NULL)
			bp_cb(BATTERY_PERCENT_LEVEL_1);
	}
}
EXPORT_SYMBOL(register_bp_thl_notify_ext);

void exec_bp_thl_callback(enum BATTERY_PERCENT_LEVEL_TAG bp_level)
{
	int i;

	if (bp_thl_data->bp_thl_stop == 1) {
		pr_info("[%s] bp_thl_data->bp_thl_stop=%d\n"
			, __func__, bp_thl_data->bp_thl_stop);
	} else {
		for (i = 0; i < BPCB_MAX_NUM; i++) {
			if (bpcb_tb[i].bpcb != NULL)
				bpcb_tb[i].bpcb(bp_level);
		}
		pr_info("[%s] bp_level=%d\n", __func__, bp_level);
	}
}

void exec_bp_thl_callback_ext(enum BATTERY_PERCENT_LEVEL_TAG bp_level)
{
	int i;

	if (bp_thl_data->bp_thl_stop == 1) {
		pr_info("[%s] bp_thl_data->bp_thl_stop=%d\n"
			, __func__, bp_thl_data->bp_thl_stop);
	} else {
		for (i = 0; i < BPCB_MAX_NUM; i++) {
			if (bpcb_tb_ext[i].bpcb != NULL)
				bpcb_tb_ext[i].bpcb(bp_level);
		}
		pr_info("[%s] bp_level=%d\n", __func__, bp_level);
	}
}

static ssize_t bp_thl_ut_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	/*show_battery_percent_protect_ut */
	pr_info("[%s] g_bp_thl_lv=%d\n",
		__func__, bp_thl_data->bp_thl_lv);
	return sprintf(buf, "%u\n", bp_thl_data->bp_thl_lv);
}

static ssize_t bp_thl_ut_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	pr_info("[%s]\n", __func__);

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "Utest", 5))
		return -EINVAL;

	if (val < BATTERY_PERCENT_LEVEL_NUM) {
		pr_info("[%s] your input is %d\n", __func__, val);
		exec_bp_thl_callback(val);
	} else {
		pr_info("[%s] wrong number (%d)\n", __func__, val);
	}
	return size;
}

static DEVICE_ATTR_RW(bp_thl_ut);

static ssize_t bp_thl_stop_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	/*show_battery_percent_protect_stop */
	pr_info("[%s] bp_thl_data->bp_thl_stop=%d\n",
		__func__, bp_thl_data->bp_thl_stop);
	return sprintf(buf, "%u\n", bp_thl_data->bp_thl_stop);
}

static ssize_t bp_thl_stop_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	pr_info("[%s]\n", __func__);

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if ((val != 0) && (val != 1))
		val = 0;

	bp_thl_data->bp_thl_stop = val;
	pr_info("[%s] bp_thl_data->bp_thl_stop=%d\n",
		__func__, bp_thl_data->bp_thl_stop);

	return size;
}

static DEVICE_ATTR_RW(bp_thl_stop);

static ssize_t bp_thl_level_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	/*show_battery_percent_protect_level */
	pr_info("[%s] bp_thl_data->bp_thl_lv=%d\n",
			__func__, bp_thl_data->bp_thl_lv);
	return sprintf(buf, "%u\n", bp_thl_data->bp_thl_lv);
}

static ssize_t bp_thl_level_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t size)
{
	/*store_battery_percent_protect_level */
	pr_info("[%s] bp_thl_data->bp_thl_lv=%d\n"
		, __func__, bp_thl_data->bp_thl_lv);

	return size;
}

static DEVICE_ATTR_RW(bp_thl_level);

int bp_notify_handler(void *unused)
{
	do {
		wait_event_interruptible(bp_notify_waiter, (bp_notify_flag == true) ||
			(bp_notify_flag_ext == true));
		__pm_stay_awake(bp_notify_lock);
		if (bp_notify_flag) {
			exec_bp_thl_callback(bp_thl_data->bp_thl_lv);
			bp_notify_flag = false;
		}
		if (bp_notify_flag_ext) {
			exec_bp_thl_callback_ext(bp_thl_data->bp_thl_lv_ext);
			bp_notify_flag_ext = false;
		}
		__pm_relax(bp_notify_lock);
	} while (!kthread_should_stop());

	return 0;
}

int bp_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;
	union power_supply_propval val;
	int ret, uisoc, bat_status;

	if (strcmp(psy->desc->name, "battery") != 0)
		return NOTIFY_DONE;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		return NOTIFY_DONE;

	uisoc = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret)
		return NOTIFY_DONE;

	bat_status = val.intval;

	if ((bat_status != POWER_SUPPLY_STATUS_CHARGING && bat_status != -1) &&
		(bp_thl_data->bp_thl_lv == BATTERY_PERCENT_LEVEL_0) &&
		(uisoc <= bp_thl_data->soc_limit && uisoc >= 0)) {
		bp_thl_data->bp_thl_lv = BATTERY_PERCENT_LEVEL_1;
		bp_notify_flag = true;
		pr_info("bp_notify called, l=%d s=%d soc=%d\n", bp_thl_data->bp_thl_lv,
			bat_status, uisoc);
	} else if (((bat_status == POWER_SUPPLY_STATUS_CHARGING) ||
		(uisoc > bp_thl_data->soc_limit)) &&
		(bp_thl_data->bp_thl_lv == BATTERY_PERCENT_LEVEL_1)) {
		bp_thl_data->bp_thl_lv = BATTERY_PERCENT_LEVEL_0;
		bp_notify_flag = true;
		pr_info("bp_notify called, l=%d s=%d soc=%d\n", bp_thl_data->bp_thl_lv,
			bat_status, uisoc);
	}

	if ((bat_status != -1) && (bp_thl_data->bp_thl_lv_ext == BATTERY_PERCENT_LEVEL_0) &&
		(uisoc <= bp_thl_data->soc_limit_ext && uisoc > 0)) {
		bp_thl_data->bp_thl_lv_ext = BATTERY_PERCENT_LEVEL_1;
		bp_notify_flag_ext = true;
		pr_info("bp_notify_ext called, l=%d s=%d soc=%d\n", bp_thl_data->bp_thl_lv_ext,
			bat_status, uisoc);
	} else if ((uisoc >= bp_thl_data->soc_limit_ext_release) &&
		(bp_thl_data->bp_thl_lv_ext == BATTERY_PERCENT_LEVEL_1)) {
		bp_thl_data->bp_thl_lv_ext = BATTERY_PERCENT_LEVEL_0;
		bp_notify_flag_ext = true;
		pr_info("bp_notify_ext called, l=%d s=%d soc=%d\n",
			bp_thl_data->bp_thl_lv_ext, bat_status, uisoc);
	}

	if (bp_notify_flag_ext || bp_notify_flag)
		wake_up_interruptible(&bp_notify_waiter);

	return NOTIFY_DONE;
}

static int bp_thl_probe(struct platform_device *pdev)
{
	struct bp_thl_priv *priv;
	struct device_node *np;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	dev_set_drvdata(&pdev->dev, priv);

	np = of_find_compatible_node(NULL, NULL, "mediatek,mtk-bp-thl");

	if (!np) {
		dev_notice(&pdev->dev, "get mtk battery oc node fail\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "soc_limit", &priv->soc_limit);
	if (ret)
		priv->soc_limit = BAT_PERCENT_LIMIT;

	ret = of_property_read_u32(np, "soc_limit_ext", &priv->soc_limit_ext);
	if (ret)
		priv->soc_limit_ext = BAT_PERCENT_LIMIT_EXT;

	ret = of_property_read_u32(np, "soc_limit_ext_release", &priv->soc_limit_ext_release);
	if (ret)
		priv->soc_limit_ext_release = BAT_PERCENT_LIMIT_RELEASE_EXT;

	bp_thl_data = priv;

	bp_notify_lock = wakeup_source_register(NULL, "bp_notify_lock wakelock");
	if (!bp_notify_lock) {
		pr_notice("bp_notify_lock wakeup source fail\n");
		return -ENOMEM;
	}

	bp_notify_thread = kthread_run(bp_notify_handler, 0, "bp_notify_thread");
	if (IS_ERR(bp_notify_thread)) {
		pr_notice("Failed to create bp_notify_thread\n");
		return PTR_ERR(bp_notify_thread);
	}

	bp_nb.notifier_call = bp_psy_event;
	ret = power_supply_reg_notifier(&bp_nb);
	if (ret) {
		pr_notice("power_supply_reg_notifier fail\n");
		return ret;
	}

	ret = device_create_file(&(pdev->dev),
		&dev_attr_bp_thl_ut);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_bp_thl_stop);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_bp_thl_level);
	if (ret)
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);

	return 0;
}

static const struct of_device_id bp_thl_of_match[] = {
	{
		.compatible = "mediatek,mtk-bp-thl",
	},
	{
	},
};

MODULE_DEVICE_TABLE(of, bp_thl_of_match);
static struct platform_driver bp_thl_driver = {
	.driver = {
		.name = "mtk_bp_thl",
		.of_match_table = bp_thl_of_match,
	},
	.probe = bp_thl_probe,
};

module_platform_driver(bp_thl_driver);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK battery percent throttling driver");
MODULE_LICENSE("GPL");
