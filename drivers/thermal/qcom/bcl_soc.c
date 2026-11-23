// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include "thermal_zone_internal.h"

#define BCL_DRIVER_NAME       "bcl_soc_peripheral"
#define RETRY_DELAY msecs_to_jiffies(1000)
#define BOOT_SOCD_THRESHOLD 97
#define MAX_STR_LEN 128

static int boot_frequency = INT_MAX;
module_param(boot_frequency, int, 0400);

struct bcl_device {
	struct device				*dev;
	struct notifier_block			psy_nb;
	struct work_struct			soc_eval_work;
	long					trip_temp;
	int					trip_val;
	struct mutex				state_trans_lock;
	bool					irq_enabled;
	bool					psy_init_done;
	bool					boot_limit_enabled;
	char					batt_model_name[MAX_STR_LEN];
	struct power_supply			*usb_psy;
	struct power_supply			*batt_psy;
	struct thermal_zone_device		*tz_dev;
	struct thermal_zone_device_ops		ops;
	struct delayed_work			register_work;
};

static struct bcl_device *bcl_perph;
static DEFINE_PER_CPU(struct freq_qos_request, cpu_qos_max_req);

static void bcl_soc_clear_cpu_boot_limit(void)
{
	struct freq_qos_request *qos_req;
	int cpu = 0;

	for_each_possible_cpu(cpu) {
		qos_req = &per_cpu(cpu_qos_max_req, cpu);
		if (freq_qos_request_active(qos_req))
			freq_qos_remove_request(qos_req);
	}
}

static int bcl_soc_trigger_cpu_boot_limit(void)
{
	int ret = 0, cpu = 0;
	struct cpufreq_policy *policy;
	struct freq_qos_request *qos_req;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_err("cpufreq policy not found for cpu%d\n", cpu);
			ret = -ESRCH;
			goto remove_qos_req;
		}

		qos_req = &per_cpu(cpu_qos_max_req, cpu);
		ret = freq_qos_add_request(&policy->constraints, qos_req,
				FREQ_QOS_MAX, boot_frequency);
		if (ret < 0) {
			pr_err("Failed to add max freq constraint (%d)\n", ret);
			cpufreq_cpu_put(policy);
			goto remove_qos_req;
		}
		cpufreq_cpu_put(policy);
	}
	pr_debug("Applied battery soc boot limit with freq:%d\n",
		boot_frequency);
	return 0;

remove_qos_req:
	bcl_soc_clear_cpu_boot_limit();
	return ret;
}

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
	if (high == bcl_perph->trip_temp)
		return 0;

	mutex_lock(&bcl_perph->state_trans_lock);
	pr_debug("socd threshold:%d\n", high);
	bcl_perph->trip_temp = high;
	if (high == INT_MAX) {
		bcl_perph->irq_enabled = false;
		goto unlock_and_exit;
	}
	bcl_perph->irq_enabled = true;
	schedule_work(&bcl_perph->soc_eval_work);

unlock_and_exit:
	mutex_unlock(&bcl_perph->state_trans_lock);
	return 0;
}

static int soc_read_usb_charger_type(void)
{
	union power_supply_propval ret = {0,};
	int err = 0;

	if (!bcl_perph->usb_psy)
		bcl_perph->usb_psy = power_supply_get_by_name("usb");
	if (bcl_perph->usb_psy) {
		err = power_supply_get_property(bcl_perph->usb_psy,
				POWER_SUPPLY_PROP_USB_TYPE, &ret);
		if (err < 0) {
			pr_err("usb charger type read error:%d\n",
				err);
			return err;
		}
	}
	pr_debug("usb charger type:%d\n", ret.intval);

	return ret.intval;
}

static int bcl_read_soc(struct thermal_zone_device *tz, int *val)
{
	union power_supply_propval ret = {0,};
	int err = 0;

	*val = 0;
	if (!bcl_perph->batt_psy)
		bcl_perph->batt_psy = power_supply_get_by_name("battery");
	if (bcl_perph->batt_psy) {
		err = power_supply_get_property(bcl_perph->batt_psy,
				POWER_SUPPLY_PROP_PRESENT, &ret);
		if (err < 0) {
			pr_err("battery presence read error:%d\n", err);
			return err;
		}

		if (ret.intval != 1) {
			pr_debug("battery is not present, defaulting into 0\n");
			return 0;
		}

		err = power_supply_get_property(bcl_perph->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (err < 0) {
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

	if (!bcl_perph->tz_dev)
		return;

	if (bcl_read_soc(NULL, &battery_depletion) < 0)
		return;

	mutex_lock(&bcl_perph->state_trans_lock);
	if (!bcl_perph->irq_enabled)
		goto eval_exit;
	if (battery_depletion < bcl_perph->trip_temp)
		goto eval_exit;

	bcl_perph->trip_val = battery_depletion;
	mutex_unlock(&bcl_perph->state_trans_lock);
	thermal_zone_device_update(bcl_perph->tz_dev,
				THERMAL_TRIP_VIOLATED);

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

static void bcl_soc_remove(struct platform_device *pdev)
{
	power_supply_unreg_notifier(&bcl_perph->psy_nb);
	flush_work(&bcl_perph->soc_eval_work);
}

static void power_supply_init_work(struct work_struct *work)
{
	int battery_depletion = 100, ret;
	static uint8_t retry_cnt = 20;
	union power_supply_propval rc = {0,};
	struct bcl_device *bcl_perph = container_of(work,
						struct bcl_device,
						register_work.work);

	if (!bcl_perph->psy_init_done) {
		bcl_perph->batt_psy = power_supply_get_by_name("battery");
		bcl_perph->usb_psy = power_supply_get_by_name("usb");
		if (!bcl_perph->batt_psy || !bcl_perph->usb_psy) {
			queue_delayed_work(system_highpri_wq,
				&bcl_perph->register_work, RETRY_DELAY);
			return;
		}
		ret = power_supply_reg_notifier(&bcl_perph->psy_nb);
		if (ret < 0) {
			pr_debug("soc notifier registration error. defer. err:%d\n",
				ret);
			if (ret == -EPROBE_DEFER && --retry_cnt) {
				queue_delayed_work(system_highpri_wq,
						&bcl_perph->register_work,
						RETRY_DELAY);
			}
			return;
		}

		bcl_perph->tz_dev = devm_thermal_of_zone_register(bcl_perph->dev,
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
		bcl_perph->psy_init_done = true;
		ret = power_supply_get_property(bcl_perph->batt_psy, POWER_SUPPLY_PROP_MODEL_NAME, &rc);
		if (ret >= 0 && rc.strval) {
		strscpy(bcl_perph->batt_model_name, rc.strval, MAX_STR_LEN);
		} else {
			strscpy(bcl_perph->batt_model_name, "Unknown", MAX_STR_LEN);
		}
	}

	if (bcl_perph->boot_limit_enabled) {
		if (bcl_read_soc(NULL, &battery_depletion) < 0)
			battery_depletion = 100;

		if (soc_read_usb_charger_type() != POWER_SUPPLY_USB_TYPE_SDP ||
			battery_depletion < BOOT_SOCD_THRESHOLD) {
			/* Clear boot limit and exit */
			bcl_soc_clear_cpu_boot_limit();
			bcl_perph->boot_limit_enabled = false;
			pr_debug(
			"Cleared boot cpu limit,socd:%d charger type:%d\n",
				battery_depletion, soc_read_usb_charger_type());
			return;
		}

		queue_delayed_work(system_highpri_wq,
			&bcl_perph->register_work, RETRY_DELAY);
	}

	return;
bcl_soc_probe_exit:
	power_supply_unreg_notifier(&bcl_perph->psy_nb);
	flush_work(&bcl_perph->soc_eval_work);
	if (bcl_perph->boot_limit_enabled) {
		bcl_soc_clear_cpu_boot_limit();
		bcl_perph->boot_limit_enabled = false;
	}
};

static int bcl_soc_apply_bootup_cpu_limit(struct bcl_device *bcl_perph)
{
	int ret;

	ret = bcl_soc_trigger_cpu_boot_limit();
	if (ret)
		return ret;

	bcl_perph->boot_limit_enabled = true;

	return 0;
}

static int bcl_soc_probe(struct platform_device *pdev)
{
	bcl_perph = devm_kzalloc(&pdev->dev, sizeof(*bcl_perph), GFP_KERNEL);
	if (!bcl_perph)
		return -ENOMEM;

	mutex_init(&bcl_perph->state_trans_lock);
	bcl_perph->dev = &pdev->dev;

	if (boot_frequency != INT_MAX)
		bcl_soc_apply_bootup_cpu_limit(bcl_perph);

	bcl_perph->ops.get_temp = bcl_read_soc;
	bcl_perph->ops.set_trips = bcl_set_soc;
	bcl_perph->ops.get_trend = bcl_soc_get_trend;
	bcl_perph->ops.change_mode = qti_tz_change_mode;
	INIT_WORK(&bcl_perph->soc_eval_work, bcl_evaluate_soc);
	bcl_perph->psy_nb.notifier_call = battery_supply_callback;

	INIT_DEFERRABLE_WORK(&bcl_perph->register_work, power_supply_init_work);
	queue_delayed_work(system_highpri_wq, &bcl_perph->register_work, 0);

	dev_set_drvdata(&pdev->dev, bcl_perph);

	return 0;
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
