// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "thermal_zone_internal.h"

#define BCL_DRIVER_NAME       "bcl_soc_peripheral"

struct bcl_soc_device {
	struct device				*dev;
	struct notifier_block			psy_nb;
	struct work_struct			soc_eval_work;
	long					trip_temp;
	int					trip_val;
	struct mutex				state_trans_lock;
	bool					irq_enabled;
	struct thermal_zone_device		*tz_dev;
	struct thermal_zone_device_ops	ops;
};

static struct bcl_soc_device *bcl_soc;

/* WA to add writable trip_temp_*_hyst sysfs node till core has proper fix */
static int bcl_soc_set_trip_hyst(
		struct thermal_zone_device *tz, int trip, int hysteresis)
{
	return 0;
};

static int bcl_soc_get_trend(struct thermal_zone_device *tz,
			const struct thermal_trip *trip,
			enum thermal_trend *trend)
{
	int trip_temp = 0, trip_hyst = 0, temp;

	if (!tz || !trip)
		return -EINVAL;

	trip_temp = trip->temperature;
	trip_hyst = trip->hysteresis;
	temp = READ_ONCE(tz->temperature);

	if (temp >= trip_temp)
		*trend = THERMAL_TREND_RAISING;
	else if (!trip_hyst && temp < trip_temp)
		*trend = THERMAL_TREND_DROPPING;
	else if (temp <= (trip_temp - trip_hyst))
		*trend = THERMAL_TREND_DROPPING;
	else
		*trend = THERMAL_TREND_STABLE;

	return 0;
}

static int bcl_set_soc(struct thermal_zone_device *tz, int low, int high)
{
	if (high == bcl_soc->trip_temp)
		return 0;

	mutex_lock(&bcl_soc->state_trans_lock);
	pr_debug("socd threshold:%d\n", high);
	bcl_soc->trip_temp = high;
	if (high == INT_MAX) {
		bcl_soc->irq_enabled = false;
		goto unlock_and_exit;
	}
	bcl_soc->irq_enabled = true;
	schedule_work(&bcl_soc->soc_eval_work);

unlock_and_exit:
	mutex_unlock(&bcl_soc->state_trans_lock);
	return 0;
}

static int bcl_read_soc(struct thermal_zone_device *tz, int *val)
{
	static struct power_supply *batt_psy;
	union power_supply_propval ret = {0,};
	int err = 0;

	*val = 0;
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
		*val = 100 - ret.intval;
	}
	pr_debug("soc:%d\n", *val);

	return err;
}

static void bcl_evaluate_soc(struct work_struct *work)
{
	int battery_depletion;

	if (!bcl_soc->tz_dev)
		return;

	if (bcl_read_soc(NULL, &battery_depletion))
		return;

	mutex_lock(&bcl_soc->state_trans_lock);
	if (!bcl_soc->irq_enabled)
		goto eval_exit;
	if (battery_depletion < bcl_soc->trip_temp)
		goto eval_exit;

	bcl_soc->trip_val = battery_depletion;
	mutex_unlock(&bcl_soc->state_trans_lock);
	thermal_zone_device_update(bcl_soc->tz_dev,
				THERMAL_TRIP_VIOLATED);

	return;
eval_exit:
	mutex_unlock(&bcl_soc->state_trans_lock);
}

static int battery_supply_callback(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (strcmp(psy->desc->name, "battery"))
		return NOTIFY_OK;
	schedule_work(&bcl_soc->soc_eval_work);

	return NOTIFY_OK;
}

static int bcl_soc_remove(struct platform_device *pdev)
{
	power_supply_unreg_notifier(&bcl_soc->psy_nb);
	flush_work(&bcl_soc->soc_eval_work);

	return 0;
}

static int bcl_soc_probe(struct platform_device *pdev)
{
	int ret = 0;

	bcl_soc = devm_kzalloc(&pdev->dev, sizeof(*bcl_soc), GFP_KERNEL);
	if (!bcl_soc)
		return -ENOMEM;

	mutex_init(&bcl_soc->state_trans_lock);
	bcl_soc->dev = &pdev->dev;
	bcl_soc->ops.get_temp = bcl_read_soc;
	bcl_soc->ops.set_trips = bcl_set_soc;
	bcl_soc->ops.set_trip_hyst = bcl_soc_set_trip_hyst;
	bcl_soc->ops.get_trend = bcl_soc_get_trend;
	bcl_soc->ops.change_mode = qti_tz_change_mode;
	INIT_WORK(&bcl_soc->soc_eval_work, bcl_evaluate_soc);
	bcl_soc->psy_nb.notifier_call = battery_supply_callback;
	ret = power_supply_reg_notifier(&bcl_soc->psy_nb);
	if (ret < 0) {
		pr_err("soc notifier registration error. defer. err:%d\n",
			ret);
		ret = -EPROBE_DEFER;
		goto bcl_soc_probe_exit;
	}
	bcl_soc->tz_dev = devm_thermal_of_zone_register(&pdev->dev,
				0, bcl_soc, &bcl_soc->ops);
	if (IS_ERR(bcl_soc->tz_dev)) {
		pr_err("soc TZ register failed. err:%ld\n",
				PTR_ERR(bcl_soc->tz_dev));
		ret = PTR_ERR(bcl_soc->tz_dev);
		bcl_soc->tz_dev = NULL;
		goto bcl_soc_probe_exit;
	}
	thermal_zone_device_update(bcl_soc->tz_dev, THERMAL_DEVICE_UP);
	schedule_work(&bcl_soc->soc_eval_work);

	dev_set_drvdata(&pdev->dev, bcl_soc);

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
		.of_match_table = bcl_match,
	},
};

module_platform_driver(bcl_driver);
MODULE_LICENSE("GPL");
