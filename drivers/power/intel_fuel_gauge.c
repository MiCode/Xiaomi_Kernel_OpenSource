/*
 * intel_fuel_gauge.c - Intel MID Fuel Gauge Driver
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *         Srinidhi Rao <srinidhi.rao@intel.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/param.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/version.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>
#include <linux/async.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/power/intel_fuel_gauge.h>
#include <asm/intel_em_config.h>

#define DRIVER_NAME	"intel_fuel_gauge"

#define INTEL_FG_WAKELOCK_TIMEOUT	(1 * HZ)
#define INTEL_FG_DISP_LOWBATT_TIMEOUT   (3 * HZ)
#define SOC_WARN_LVL1			14
#define SOC_WARN_LVL2			4
#define SOC_WARN_LVL3			0

#define BATT_OVP_OFFSET			50000 /* 50mV */

#define FG_ADC_VBATT_OFF_ADJ		5000 /* 5mV */
#define FG_ADC_IBATT_OFF_ADJ		30000 /* 30mA */

#define FG_OCV_SMOOTH_DIV_NOR		20
#define FG_OCV_SMOOTH_DIV_FULL		100
#define FG_OCV_SMOOTH_CAP_LIM		97

#define BOUND(min_val, x, max_val) min(max(x, min_val), max_val)

struct intel_fg_wakeup_event {
	int soc_bfr_sleep;
	bool wake_enable;
	struct wake_lock wakelock;
};
struct intel_fg_info {
	struct device *dev;
	struct intel_fg_batt_spec *batt_spec;
	struct intel_fg_input *input;
	struct intel_fg_algo *algo;
	struct intel_fg_algo *algo_sec;
	struct delayed_work fg_worker;
	struct power_supply psy;
	struct mutex lock;

	struct intel_fg_wakeup_event wake_ui;
	struct fg_batt_params batt_params;
};

static struct intel_fg_info *info_ptr;

/* default battery spec data */
static struct intel_fg_batt_spec bspec = {
	.volt_min_design = 3400000,
	.volt_max_design = 4350000,
	.temp_min = 0,
	.temp_max = 450,
	.charge_full_design = 4980000,
};

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

/**
 * intel_fg_check_low_batt_event - Checks low batt condition
 * @info : Pointer to the intel_fg_info structure instance
 *
 * Returns 0 if success
 */
static int intel_fg_check_low_batt_event(struct intel_fg_info *info)
{
	int ret = 0;

	/*
	 * Compare the previously stored capacity before going to suspend mode,
	 * with the current capacity during resume, along with the SOC_WARN_LVLs
	 * and if the new SOC during resume has fell below any of the low batt
	 * warning levels, hold the wake lock for 1 sec so that Android user
	 * space will have sufficient time to display the warning message.
	 */
	if (BOUND(info->batt_params.capacity, SOC_WARN_LVL1,
				info->wake_ui.soc_bfr_sleep) == SOC_WARN_LVL1)
		info->wake_ui.wake_enable = true;
	else if (BOUND(info->batt_params.capacity, SOC_WARN_LVL2,
				info->wake_ui.soc_bfr_sleep) == SOC_WARN_LVL2)
		info->wake_ui.wake_enable = true;
	else if (info->batt_params.capacity == SOC_WARN_LVL3)
		info->wake_ui.wake_enable = true;
	else {
		if (wake_lock_active(&info_ptr->wake_ui.wakelock))
			wake_unlock(&info_ptr->wake_ui.wakelock);
		info->wake_ui.wake_enable = false;
	}

	if (info->wake_ui.wake_enable) {
		wake_lock_timeout(&info_ptr->wake_ui.wakelock,
			INTEL_FG_DISP_LOWBATT_TIMEOUT);
		info->wake_ui.wake_enable = false;
	}
	return ret;
}
static int intel_fg_vbatt_soc_calc(struct intel_fg_info *info, int vbatt)
{
	int soc;

	soc = (vbatt - info->batt_spec->volt_min_design) * 100;
	soc /= (info->batt_spec->volt_max_design -
			info->batt_spec->volt_min_design);

	/* limit the capacity to 0 to 100 */
	soc = clamp(soc, 0, 100);

	return soc;
}

static int intel_fg_apply_volt_smooth(int vocv, int vbatt, int ibatt, int cap)
{
	static int vsocv = -1;
	int vdiff;

	if (vsocv == -1) {
		vsocv = vocv;
		return vsocv;
	}

	/*
	 * for fully charged battery vbatt
	 * and ocv should be same.
	 */
	if (cap >= FG_OCV_SMOOTH_CAP_LIM &&
			ibatt >= FG_ADC_IBATT_OFF_ADJ)
		vocv = vbatt;

	vdiff = vocv - vsocv;
	/*
	 * handle leakage current or CC errors
	 * scenarios for OCV calculation.
	 */
	if ((ibatt > -FG_ADC_IBATT_OFF_ADJ &&
		ibatt < FG_ADC_IBATT_OFF_ADJ) && (cap < 100)) {
		vsocv += vdiff / FG_OCV_SMOOTH_DIV_FULL;
		return vsocv;
	}

	/* round off to +/- 5000uV */
	if (vdiff <= FG_ADC_VBATT_OFF_ADJ &&
			vdiff >= -FG_ADC_VBATT_OFF_ADJ)
		vsocv = vocv;
	else
		vsocv += vdiff / FG_OCV_SMOOTH_DIV_NOR;

	return vsocv;
}

static void intel_fg_worker(struct work_struct *work)
{
	struct intel_fg_info *fg_info = container_of(work,
				struct intel_fg_info, fg_worker);
	struct fg_algo_ip_params ip;
	struct fg_algo_op_params op;
	int ret;

	memset(&op, 0, sizeof(struct fg_algo_op_params));

	mutex_lock(&fg_info->lock);

	ret = fg_info->input->get_delta_q(&ip.delta_q);
	if (ret)
		dev_err(fg_info->dev, "Error while getting delta Q\n");

	ret = fg_info->input->get_batt_params(&ip.vbatt,
					&ip.ibatt, &ip.bat_temp);
	if (ret)
		dev_err(fg_info->dev, "Error while getting battery props\n");

	ret = fg_info->input->get_v_avg(&ip.vavg);
	if (ret)
		dev_err(fg_info->dev, "Error while getting V-AVG\n");

	ret = fg_info->input->get_v_ocv(&ip.vocv);
	if (ret)
		dev_err(fg_info->dev, "Error while getting OCV\n");

	ret = fg_info->input->get_i_avg(&ip.iavg);
	if (ret)
		dev_err(fg_info->dev, "Error while getting Current Average\n");

	mutex_unlock(&fg_info->lock);
	if (fg_info->algo) {
		ret = fg_info->algo->fg_algo_process(&ip, &op);
		mutex_lock(&fg_info->lock);
		if (ret) {
			dev_err(fg_info->dev,
				"Err processing FG Algo primary\n");
			fg_info->batt_params.capacity =
				intel_fg_vbatt_soc_calc(fg_info, ip.vocv);
		} else {
			/* update battery parameters */
			fg_info->batt_params.capacity = op.soc;
			fg_info->batt_params.charge_now = op.nac;
			fg_info->batt_params.charge_full = op.fcc;
		}

	} else if (fg_info->algo_sec) {
		ret = fg_info->algo_sec->fg_algo_process(&ip, &op);
		mutex_lock(&fg_info->lock);
		if (ret)
			dev_err(fg_info->dev,
				"Err processing FG Algo Secondary\n");
		/* update battery parameters from secondary Algo*/
		fg_info->batt_params.capacity = op.soc;
		fg_info->batt_params.charge_now = op.nac;
		fg_info->batt_params.charge_full = op.fcc;
	}
	fg_info->batt_params.vbatt_now = ip.vbatt;
	fg_info->batt_params.v_ocv_now =
		intel_fg_apply_volt_smooth(ip.vocv, ip.vbatt, ip.ibatt, op.soc);
	fg_info->batt_params.i_batt_now = ip.ibatt;
	fg_info->batt_params.i_batt_avg = ip.iavg;
	fg_info->batt_params.batt_temp_now = ip.bat_temp;
	fg_info->batt_params.charge_counter += ip.delta_q;
	if (op.calib_cc) {
		ret = fg_info->input->calibrate_cc();
		if (ret)
			dev_err(fg_info->dev,
				"error while calibrating CC\n");
	}

	mutex_unlock(&fg_info->lock);
	power_supply_changed(&fg_info->psy);
	if (fg_info->wake_ui.wake_enable)
		intel_fg_check_low_batt_event(fg_info);
	schedule_delayed_work(&fg_info->fg_worker, 30 * HZ);
}

static int intel_fg_battery_health(struct intel_fg_info *info)
{
	struct fg_batt_params *bat = &info->batt_params;
	int health;


	if (!info->batt_params.is_valid_battery)
		health = POWER_SUPPLY_HEALTH_UNKNOWN;
	else if (bat->vbatt_now > info->batt_spec->volt_max_design
			+ BATT_OVP_OFFSET)
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else if (bat->batt_temp_now > info->batt_spec->temp_max ||
			bat->batt_temp_now < info->batt_spec->temp_min)
		health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (bat->v_ocv_now < info->batt_spec->volt_min_design)
		health = POWER_SUPPLY_HEALTH_DEAD;
	else
		health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static int intel_fuel_gauge_get_property(struct power_supply *psup,
					enum power_supply_property prop,
					union power_supply_propval *val)
{
	struct intel_fg_info *fg_info = container_of(psup,
					struct intel_fg_info, psy);

	mutex_lock(&fg_info->lock);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = fg_info->batt_params.status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 0x1;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = intel_fg_battery_health(fg_info);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = fg_info->batt_params.vbatt_now;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = fg_info->batt_params.v_ocv_now;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = fg_info->batt_spec->volt_min_design;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = fg_info->batt_spec->volt_max_design;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = fg_info->batt_params.i_batt_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = fg_info->batt_params.i_batt_avg;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (fg_info->algo || fg_info->algo_sec)
			val->intval = fg_info->batt_params.capacity;
		else
			val->intval = intel_fg_vbatt_soc_calc(fg_info,
				fg_info->batt_params.v_ocv_now);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = fg_info->batt_params.batt_temp_now;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = fg_info->batt_params.charge_now;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = fg_info->batt_params.charge_full;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = fg_info->batt_spec->charge_full_design;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fg_info->batt_params.charge_counter;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "INTN0001";
		break;
	default:
		mutex_unlock(&fg_info->lock);
		return -EINVAL;
	}

	mutex_unlock(&fg_info->lock);
	return 0;
}

static int intel_fuel_gauge_set_property(struct power_supply *psup,
				enum power_supply_property prop,
				const union power_supply_propval *val)
{
	struct intel_fg_info *fg_info = container_of(psup,
						struct intel_fg_info, psy);

	mutex_lock(&fg_info->lock);
	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		fg_info->batt_params.status = val->intval;
		break;
	default:
		dev_warn(fg_info->dev, "invalid psy prop\b");
		mutex_unlock(&fg_info->lock);
		return -EINVAL;
	}
	mutex_unlock(&fg_info->lock);
	return 0;
}

static void intel_fg_ext_psy_changed(struct power_supply *psy)
{
	struct intel_fg_info *fg_info = container_of(psy,
					struct intel_fg_info, psy);

	dev_info(fg_info->dev, "%s\n", __func__);
	power_supply_changed(&fg_info->psy);
}

static void intel_fg_init_batt_props(struct intel_fg_info *fg_info)
{
	struct fg_algo_ip_params ip;
	int ret;

	ret = fg_info->input->get_batt_params(&ip.vbatt,
					&ip.ibatt, &ip.bat_temp);
	if (ret)
		dev_err(fg_info->dev, "Error while getting battery props\n");

	ret = fg_info->input->get_v_ocv(&ip.vocv);
	if (ret)
		dev_err(fg_info->dev, "\nError while getting OCV");

	ret = fg_info->input->get_i_avg(&ip.iavg);
	if (ret)
		dev_err(fg_info->dev, "\nError while getting Current Average");

	fg_info->batt_params.capacity = intel_fg_vbatt_soc_calc(fg_info,
								ip.vocv);
	fg_info->batt_params.vbatt_now = ip.vbatt;
	fg_info->batt_params.v_ocv_now = ip.vocv;
	fg_info->batt_params.i_batt_now = ip.ibatt;
	fg_info->batt_params.i_batt_avg = ip.iavg;
	fg_info->batt_params.batt_temp_now = ip.bat_temp;
}

static void intel_fuel_gauge_algo_init(struct intel_fg_info *fg_info)
{
	int ret;

	ret = fg_info->input->get_v_ocv_bootup(
			&fg_info->batt_params.v_ocv_bootup);
	if (ret)
		dev_err(fg_info->dev, "error in getting bootup voltage\n");
	else
		dev_info(fg_info->dev, "boot up voltage:%d\n",
					fg_info->batt_params.v_ocv_bootup);

	ret = fg_info->input->get_i_bat_bootup(
			&fg_info->batt_params.i_bat_bootup);
	if (ret)
		dev_err(fg_info->dev, "error in getting bootup ibat\n");
	else
		dev_info(fg_info->dev, "boot up ibat:%d\n",
					fg_info->batt_params.i_bat_bootup);

	/* update battery adc params */
	intel_fg_init_batt_props(info_ptr);

	fg_info->batt_params.boot_flag = true;
	if (fg_info->algo && !fg_info->algo->init_done) {
		fg_info->algo->fg_algo_init(&fg_info->batt_params);
		fg_info->algo->init_done = true;
	} else if (fg_info->algo_sec && !fg_info->algo_sec->init_done) {
		fg_info->algo_sec->fg_algo_init(&fg_info->batt_params);
		fg_info->algo_sec->init_done = true;
	}
		fg_info->batt_params.boot_flag = false;

	schedule_delayed_work(&fg_info->fg_worker, 20 * HZ);
}

int intel_fg_register_input(struct intel_fg_input *input)
{
	int ret;

	if (!info_ptr)
		return -EAGAIN;

	mutex_lock(&info_ptr->lock);

	info_ptr->input = input;
	/* init fuel gauge lib's or algo's */
	if (info_ptr->algo || info_ptr->algo_sec)
		intel_fuel_gauge_algo_init(info_ptr);
	else
		intel_fg_init_batt_props(info_ptr);

	mutex_unlock(&info_ptr->lock);

	info_ptr->psy.name = "intel_fuel_gauge";
	info_ptr->psy.type = POWER_SUPPLY_TYPE_BATTERY;
	info_ptr->psy.get_property = &intel_fuel_gauge_get_property;
	info_ptr->psy.set_property = &intel_fuel_gauge_set_property;
	info_ptr->psy.external_power_changed = &intel_fg_ext_psy_changed;
	info_ptr->psy.properties = &fg_props;
	info_ptr->psy.num_properties = ARRAY_SIZE(fg_props);

	ret = power_supply_register(info_ptr->dev, &info_ptr->psy);
	if (ret) {
		dev_err(info_ptr->dev, "power supply class reg failed\n");
		return ret;
	}
	/*Start Coulomb Counter Calibration*/
	ret = info_ptr->input->calibrate_cc();
	if (ret)
		dev_err(info_ptr->dev, "error in calibrating CC\n");
	/*If No FG Algo has been registered, schedule the worker thread
		upon input driver registration*/
	if (!info_ptr->algo && !info_ptr->algo_sec)
		schedule_delayed_work(&info_ptr->fg_worker, 40 * HZ);
	return 0;
}
EXPORT_SYMBOL(intel_fg_register_input);

int intel_fg_unregister_input(struct intel_fg_input *input)
{
	if (!info_ptr || !info_ptr->input)
		return -ENODEV;

	flush_scheduled_work();
	power_supply_unregister(&info_ptr->psy);

	mutex_lock(&info_ptr->lock);
	info_ptr->input = NULL;
	mutex_unlock(&info_ptr->lock);

	return 0;
}
EXPORT_SYMBOL(intel_fg_unregister_input);

int intel_fg_register_algo(struct intel_fg_algo *algo)
{
	if (!info_ptr)
		return -EAGAIN;

	mutex_lock(&info_ptr->lock);
	if (algo->type == INTEL_FG_ALGO_PRIMARY) {
		if (!info_ptr->algo)
			info_ptr->algo = algo;
		else
			goto register_algo;
	} else {
		if (!info_ptr->algo_sec)
			info_ptr->algo_sec = algo;
		else
			goto register_algo;
	}

	/* init fuel gauge lib's or algo's */
	if (info_ptr->input)
		intel_fuel_gauge_algo_init(info_ptr);

	mutex_unlock(&info_ptr->lock);
	return 0;

register_algo:
	mutex_unlock(&info_ptr->lock);
	return -EBUSY;
}
EXPORT_SYMBOL(intel_fg_register_algo);

int intel_fg_unregister_algo(struct intel_fg_algo *algo)
{
	if (!info_ptr)
		return -ENODEV;

	mutex_lock(&info_ptr->lock);
	if (algo->type == INTEL_FG_ALGO_PRIMARY) {
		if (info_ptr->algo)
			info_ptr->algo = NULL;
		else
			goto unregister_algo;
	} else {
		if (!info_ptr->algo_sec)
			info_ptr->algo_sec = NULL;
		else
			goto unregister_algo;
	}

	mutex_unlock(&info_ptr->lock);
	return 0;

unregister_algo:
	mutex_unlock(&info_ptr->lock);
	return -ENODEV;
}
EXPORT_SYMBOL(intel_fg_unregister_algo);

static int intel_fuel_gauge_probe(struct platform_device *pdev)
{
	struct intel_fg_info *fg_info;
	struct em_config_oem0_data oem0_data;

	fg_info = devm_kzalloc(&pdev->dev, sizeof(*fg_info), GFP_KERNEL);
	if (!fg_info) {
		dev_err(&pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	fg_info->dev = &pdev->dev;
	fg_info->batt_spec = &bspec;
	platform_set_drvdata(pdev, fg_info);

	mutex_init(&fg_info->lock);
	INIT_DELAYED_WORK(&fg_info->fg_worker, &intel_fg_worker);
	fg_info->batt_params.status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (em_config_get_oem0_data(&oem0_data))
		fg_info->batt_params.is_valid_battery = true;
	else
		fg_info->batt_params.is_valid_battery = false;

	wake_lock_init(&fg_info->wake_ui.wakelock, WAKE_LOCK_SUSPEND,
				"intel_fg_wakelock");

	info_ptr = fg_info;

	return 0;
}

static int intel_fuel_gauge_remove(struct platform_device *pdev)
{
	struct intel_fg_info *fg_info = platform_get_drvdata(pdev);
	wake_lock_destroy(&fg_info->wake_ui.wakelock);

	return 0;
}

static int intel_fuel_gauge_suspend(struct device *dev)
{
	/*
	 * Store the current SOC value before going to suspend as
	 * this value will be used by the worker function in resume to
	 * check whether the low battery threshold has been crossed.
	 */
	info_ptr->wake_ui.soc_bfr_sleep = info_ptr->batt_params.capacity;
	cancel_delayed_work_sync(&info_ptr->fg_worker);
	return 0;
}

static int intel_fuel_gauge_resume(struct device *dev)
{
	/*
	 * Set the wake_enable flag as true and schedule the
	 * work queue at 0 secs so that the worker function is
	 * scheduled immediately at the next available tick.
	 * Once the intel_fg_worker function starts executing
	 * It can check and clear the wake_enable flag and hold
	 * the wakelock if low batt warning notification has to
	 * be sent
	 */
	wake_lock_timeout(&info_ptr->wake_ui.wakelock,
			  INTEL_FG_WAKELOCK_TIMEOUT);
	info_ptr->wake_ui.wake_enable = true;
	schedule_delayed_work(&info_ptr->fg_worker, 0);
	return 0;
}

static int intel_fuel_gauge_runtime_suspend(struct device *dev)
{
	return 0;
}
static int intel_fuel_gauge_runtime_resume(struct device *dev)
{
	return 0;
}
static int intel_fuel_gauge_runtime_idle(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops intel_fuel_gauge_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(intel_fuel_gauge_suspend,
			intel_fuel_gauge_resume)
	SET_RUNTIME_PM_OPS(intel_fuel_gauge_runtime_suspend,
			intel_fuel_gauge_runtime_resume,
			intel_fuel_gauge_runtime_idle)
};

static const struct platform_device_id intel_fuel_gauge_id[] = {
	{DRIVER_NAME, },
	{ },
};
MODULE_DEVICE_TABLE(platform, intel_fuel_gauge_id);

static struct platform_driver intel_fuel_gauge_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &intel_fuel_gauge_pm_ops,
	},
	.probe = intel_fuel_gauge_probe,
	.remove = intel_fuel_gauge_remove,
	.id_table = intel_fuel_gauge_id,
};

static int __init intel_fuel_gauge_init(void)
{
	return platform_driver_register(&intel_fuel_gauge_driver);
}
module_init(intel_fuel_gauge_init);

static void __exit intel_fuel_gauge_exit(void)
{
	platform_driver_unregister(&intel_fuel_gauge_driver);
}
module_exit(intel_fuel_gauge_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_AUTHOR("Srinidhi Rao <srinidhi.rao@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel MID Fuel Gauge Driver");
