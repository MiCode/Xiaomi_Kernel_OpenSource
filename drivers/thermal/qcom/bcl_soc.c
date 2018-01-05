/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>

#include "../thermal_core.h"

#define BCL_DRIVER_NAME       "bcl_soc_peripheral"

struct bcl_device {
	struct notifier_block			psy_nb;
	struct work_struct			soc_eval_work;
	long int				trip_temp;
	int					trip_val;
	struct mutex				state_trans_lock;
	bool					irq_enabled;
	struct thermal_zone_device		*tz_dev;
	struct thermal_zone_of_device_ops	ops;
};

static struct bcl_device *bcl_perph;

static int bcl_set_soc(void *data, int low, int high)
{
	if (low == bcl_perph->trip_temp)
		return 0;

	mutex_lock(&bcl_perph->state_trans_lock);
	pr_debug("low soc threshold:%d\n", low);
	bcl_perph->trip_temp = low;
	if (low == INT_MIN) {
		bcl_perph->irq_enabled = false;
		goto unlock_and_exit;
	}
	bcl_perph->irq_enabled = true;
	schedule_work(&bcl_perph->soc_eval_work);

unlock_and_exit:
	mutex_unlock(&bcl_perph->state_trans_lock);
	return 0;
}

static int bcl_read_soc(void *data, int *val)
{
	static struct power_supply *batt_psy;
	union power_supply_propval ret = {0,};
	int err = 0;

	*val = 100;
	if (!batt_psy)
		batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		err = power_supply_get_property(batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (err) {
			pr_err("battery percentage read error:%d\n",
				err);
			return err;
		}
		*val = ret.intval;
	}
	pr_debug("soc:%d\n", *val);

	return err;
}

static void bcl_evaluate_soc(struct work_struct *work)
{
	int battery_percentage;

	if (bcl_read_soc(NULL, &battery_percentage))
		return;

	mutex_lock(&bcl_perph->state_trans_lock);
	if (!bcl_perph->irq_enabled)
		goto eval_exit;
	if (battery_percentage > bcl_perph->trip_temp)
		goto eval_exit;

	bcl_perph->trip_val = battery_percentage;
	mutex_unlock(&bcl_perph->state_trans_lock);
	of_thermal_handle_trip(bcl_perph->tz_dev);

	return;
eval_exit:
	mutex_unlock(&bcl_perph->state_trans_lock);
}

static int battery_supply_callback(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (strcmp(psy->desc->name, "battery"))
		return NOTIFY_OK;
	schedule_work(&bcl_perph->soc_eval_work);

	return NOTIFY_OK;
}

static int bcl_soc_remove(struct platform_device *pdev)
{
	power_supply_unreg_notifier(&bcl_perph->psy_nb);
	flush_work(&bcl_perph->soc_eval_work);
	if (bcl_perph->tz_dev)
		thermal_zone_of_sensor_unregister(&pdev->dev,
				bcl_perph->tz_dev);

	return 0;
}

static int bcl_soc_probe(struct platform_device *pdev)
{
	int ret = 0;

	bcl_perph = devm_kzalloc(&pdev->dev, sizeof(*bcl_perph), GFP_KERNEL);
	if (!bcl_perph)
		return -ENOMEM;

	mutex_init(&bcl_perph->state_trans_lock);
	bcl_perph->ops.get_temp = bcl_read_soc;
	bcl_perph->ops.set_trips = bcl_set_soc;
	INIT_WORK(&bcl_perph->soc_eval_work, bcl_evaluate_soc);
	bcl_perph->psy_nb.notifier_call = battery_supply_callback;

	ret = power_supply_reg_notifier(&bcl_perph->psy_nb);
	if (ret < 0) {
		pr_err("soc notifier registration error. defer. err:%d\n",
			ret);
		ret = -EPROBE_DEFER;
		goto bcl_soc_probe_exit;
	}
	bcl_perph->tz_dev = thermal_zone_of_sensor_register(&pdev->dev,
				0, bcl_perph, &bcl_perph->ops);
	if (IS_ERR(bcl_perph->tz_dev)) {
		pr_err("soc TZ register failed. err:%ld\n",
				PTR_ERR(bcl_perph->tz_dev));
		ret = PTR_ERR(bcl_perph->tz_dev);
		bcl_perph->tz_dev = NULL;
		goto bcl_soc_probe_exit;
	}
	thermal_zone_device_update(bcl_perph->tz_dev, THERMAL_DEVICE_UP);
	schedule_work(&bcl_perph->soc_eval_work);

	dev_set_drvdata(&pdev->dev, bcl_perph);

	return 0;

bcl_soc_probe_exit:
	bcl_soc_remove(pdev);
	return ret;
}

static const struct of_device_id bcl_match[] = {
	{
		.compatible = "qcom,msm-bcl-soc",
	},
	{},
};

static struct platform_driver bcl_driver = {
	.probe  = bcl_soc_probe,
	.remove = bcl_soc_remove,
	.driver = {
		.name           = BCL_DRIVER_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = bcl_match,
	},
};

builtin_platform_driver(bcl_driver);
