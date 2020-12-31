// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "apusys_power_user.h"
#include "apusys_power.h"
#include "apu_common.h"
#include "apu_log.h"
#include "apu_plat.h"

static LIST_HEAD(power_callback_device_list);
static struct mutex power_device_list_mtx;

struct power_callback_device {
	enum POWER_CALLBACK_USER power_callback_usr;
	void (*power_on_callback)(void *para);
	void (*power_off_callback)(void *para);
	struct list_head list;
};


struct power_callback_device *_find_cb_by_user(enum POWER_CALLBACK_USER user)
{
	struct power_callback_device *pwr_dev = NULL;

	if (!list_empty(&power_callback_device_list)) {
		list_for_each_entry(pwr_dev,
				&power_callback_device_list, list) {
			if (pwr_dev && pwr_dev->power_callback_usr == user)
				return pwr_dev;
		}
	} else {
		pr_info("%s empty list\n", __func__);
	}

	return NULL;
}


int apu_power_cb_register(enum POWER_CALLBACK_USER user,
					void (*power_on_callback)(void *para),
					void (*power_off_callback)(void *para))
{
	struct power_callback_device *pwr_dev = NULL;

	pwr_dev = kzalloc(sizeof(struct power_callback_device), GFP_KERNEL);
	if (!pwr_dev)
		return -ENOMEM;

	pwr_dev->power_callback_usr = user;
	pwr_dev->power_on_callback = power_on_callback;
	pwr_dev->power_off_callback = power_off_callback;

	/* add to device link list */
	mutex_lock(&power_device_list_mtx);
	list_add_tail(&pwr_dev->list, &power_callback_device_list);
	mutex_unlock(&power_device_list_mtx);
	return 0;
}

void apu_power_cb_unregister(enum POWER_CALLBACK_USER user)
{
	struct power_callback_device *pwr_dev = _find_cb_by_user(user);

	mutex_lock(&power_device_list_mtx);
	/* remove from device link list */
	list_del_init(&pwr_dev->list);
	kfree(pwr_dev);
	mutex_unlock(&power_device_list_mtx);
}

#if IS_ENABLED(CONFIG_PM)
static int runtime_suspend(struct device *dev)
{
	struct power_callback_device *pwr_dev = NULL;

	if (!list_empty(&power_callback_device_list))
		list_for_each_entry(pwr_dev, &power_callback_device_list, list)
			if (pwr_dev && pwr_dev->power_off_callback)
				pwr_dev->power_off_callback(NULL);
	return 0;
}

static int runtime_resume(struct device *dev)
{
	struct power_callback_device *pwr_dev = NULL;

	if (!list_empty(&power_callback_device_list))
		list_for_each_entry(pwr_dev, &power_callback_device_list, list)
			if (pwr_dev && pwr_dev->power_on_callback)
				pwr_dev->power_on_callback(NULL);
	return 0;
}

const static struct dev_pm_ops cb_pm_ops = {
	SET_RUNTIME_PM_OPS(runtime_suspend, runtime_resume, NULL)
};

#else
const static struct dev_pm_ops cb_pm_ops = {};
#endif
#define APU_CB_PM_OPS (&cb_pm_ops)

static int apu_cb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct apu_dev *ad = NULL;
	const struct apu_plat_data *apu_data = NULL;
	int err = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	mutex_init(&power_device_list_mtx);
	apu_data = of_device_get_match_data(&pdev->dev);
	if (!apu_data) {
		dev_info(dev, " has no platform data, ret %d\n", err);
		return -ENODEV;
	}

	ad = devm_kzalloc(dev, sizeof(*ad), GFP_KERNEL);
	if (!ad)
		return -ENOMEM;
	ad->dev = dev;
	ad->user = apu_data->user;
	ad->name = apu_dev_string(apu_data->user);
	/* save apu_dev to dev->driver_data */
	platform_set_drvdata(pdev, ad);

	err = apu_add_devfreq(ad);
	if (err)
		goto free_ad;

	/* initial run time power management */
	pm_runtime_enable(dev);
	return err;
free_ad:
	devm_kfree(dev, ad);
	return err;
}


static int apu_cb_remove(struct platform_device *pdev)
{
	struct apu_dev *ad = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s\n", __func__);
	/* remove apu_device from list */
	apu_del_devfreq(ad);
	return 0;
}

static const struct apu_plat_data apusys_cb_data = {
	.user = APUCB,
};

static const struct of_device_id cb_of_match[] = {
	{ .compatible = "mediatek,apusys_cb", .data = &apusys_cb_data },
	{ },
};

MODULE_DEVICE_TABLE(of, cb_of_match);

struct platform_driver apu_cb_driver = {
	.probe	= apu_cb_probe,
	.remove	= apu_cb_remove,
	.driver = {
		.name = "mediatek,apusys_cb",
		.pm = APU_CB_PM_OPS,
		.of_match_table = cb_of_match,
	},
};

