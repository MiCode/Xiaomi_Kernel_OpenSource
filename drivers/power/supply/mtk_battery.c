// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/cdev.h>		/* cdev */
#include <linux/err.h>	/* IS_ERR, PTR_ERR */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/kernel.h>
#include <linux/kthread.h>	/* For Kthread_run */
#include <linux/math64.h> /*div_s64*/
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/netlink.h>	/* netlink */
#include <linux/of_device.h>
#include <linux/of_fdt.h>	/*of_dt API*/
#include <linux/platform_device.h>	/* platform device */
#include <linux/proc_fs.h>
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/sched.h>	/* For wait queue*/
#include <linux/skbuff.h>	/* netlink */
#include <linux/socket.h>	/* netlink */
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>		/* For wait queue*/
#include <net/sock.h>		/* netlink */
#include "mtk_battery.h"

/* ============================================================ */
/* gaugel hal interface */
/* ============================================================ */
int gauge_get_property(enum gauge_property gp,
	int *val)
{
	struct mtk_gauge *gauge;
	struct power_supply *psy;
	struct mtk_gauge_sysfs_field_info *attr;

	psy = power_supply_get_by_name("mtk-gauge");
	if (psy == NULL)
		return -ENODEV;

	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
	attr = gauge->attr;

	if (attr[gp].prop == gp) {
		mutex_lock(&gauge->ops_lock);
		attr[gp].get(gauge, &attr[gp], val);
		mutex_unlock(&gauge->ops_lock);
	} else {
		dev_notice(&psy->dev, "%s gp:%d idx error\n", __func__, gp);
		return -ENOTSUPP;
	}

	return 0;
}

int gauge_get_int_property(enum gauge_property gp)
{
	int val;

	gauge_get_property(gp, &val);
	return val;
}

int gauge_set_property(enum gauge_property gp,
	int val)
{
	struct mtk_gauge *gauge;
	struct power_supply *psy;
	struct mtk_gauge_sysfs_field_info *attr;

	psy = power_supply_get_by_name("mtk-gauge");
	if (psy == NULL)
		return -ENODEV;

	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
	attr = gauge->attr;

	if (attr[gp].prop == gp) {
		mutex_lock(&gauge->ops_lock);
		attr[gp].set(gauge, &attr[gp], val);
		mutex_unlock(&gauge->ops_lock);
	} else {
		dev_notice(&psy->dev, "%s gp:%d idx error\n", __func__, gp);
		return -ENOTSUPP;
	}

	return 0;
}

/* ============================================================ */
/* power supply: battery */
/* ============================================================ */
static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
};

static int battery_psy_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0;
	struct mtk_battery *gm;
	struct battery_data *bs_data;

	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);
	bs_data = &gm->bs_data;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bs_data->bat_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bs_data->bat_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bs_data->bat_present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = bs_data->bat_technology;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (gm->fixed_uisoc != 0xffff)
			val->intval = gm->fixed_uisoc;
		else
			val->intval = bs_data->bat_capacity;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval =
			gauge_get_int_property(GAUGE_PROP_BATTERY_CURRENT)
			* 100;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval =
			gauge_get_int_property(GAUGE_PROP_BATTERY_CURRENT)
			* 100;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval =
			gm->fg_table_cust_data.fg_profile[
				gm->battery_id].q_max * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = gm->ui_soc *
			gm->fg_table_cust_data.fg_profile[
				gm->battery_id].q_max * 1000 / 100;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bs_data->bat_batt_vol * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bs_data->bat_batt_temp * 10;
		break;
	default:
		ret = -EINVAL;
		break;
		}

	return ret;
}

void battery_service_data_init(struct mtk_battery *gm)
{
	struct battery_data *bs_data;

	bs_data = &gm->bs_data;
	bs_data->psd.name = "battery",
	bs_data->psd.type = POWER_SUPPLY_TYPE_BATTERY;
	bs_data->psd.properties = battery_props,
	bs_data->psd.num_properties = ARRAY_SIZE(battery_props),
	bs_data->psd.get_property = battery_psy_get_property,
	bs_data->psy_cfg.drv_data = gm;

	bs_data->bat_status = POWER_SUPPLY_STATUS_DISCHARGING,
	bs_data->bat_health = POWER_SUPPLY_HEALTH_GOOD,
	bs_data->bat_present = 1,
	bs_data->bat_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	bs_data->bat_capacity = -1,
	bs_data->bat_batt_vol = 0,
	bs_data->bat_batt_temp = 0,

	gm->fixed_uisoc = 0xffff;
}

/* ============================================================ */
/* voltage to battery temperature */
/* ============================================================ */
int BattThermistorConverTemp(struct mtk_battery *gm, int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;
	struct fuelgauge_temperature *ptable;

	ptable = gm->tmp_table;
	if (Res >= ptable[0].TemperatureR) {
		TBatt_Value = -40;
	} else if (Res <= ptable[20].TemperatureR) {
		TBatt_Value = 60;
	} else {
		RES1 = ptable[0].TemperatureR;
		TMP1 = ptable[0].BatteryTemp;

		for (i = 0; i <= 20; i++) {
			if (Res >= ptable[i].TemperatureR) {
				RES2 = ptable[i].TemperatureR;
				TMP2 = ptable[i].BatteryTemp;
				break;
			}
			{	/* hidden else */
				RES1 = ptable[i].TemperatureR;
				TMP1 = ptable[i].BatteryTemp;
			}
		}

		TBatt_Value = (((Res - RES2) * TMP1) +
			((RES1 - Res) * TMP2)) / (RES1 - RES2);
	}
	dev_info(gm->dev, "[%s] %d %d %d %d %d %d\n",
		__func__,
		RES1, RES2, Res, TMP1,
		TMP2, TBatt_Value);

	return TBatt_Value;
}

int BattVoltToTemp(struct mtk_battery *gm, int dwVolt, int volt_cali)
{
	long long TRes_temp;
	long long TRes;
	int sBaTTMP = -100;
	int vbif28 = gm->rbat.rbat_pull_up_volt;
	int delta_v;
	int vbif28_raw;
	int ret;

	TRes_temp = (gm->rbat.rbat_pull_up_r * (long long) dwVolt);
	ret = gauge_get_property(GAUGE_PROP_BIF_VOLTAGE,
		&vbif28_raw);

	if (ret != -ENOTSUPP) {
		vbif28 = vbif28_raw + volt_cali;
		delta_v = abs(vbif28 - dwVolt);
		if (delta_v == 0)
			delta_v = 1;
#if defined(__LP64__) || defined(_LP64)
			do_div(TRes_temp, delta_v);
#else
			TRes_temp = div_s64(TRes_temp, delta_v);
#endif
		if (vbif28 > 3000 || vbif28 < 2500)
			dev_notice(gm->dev, "[RBAT_PULL_UP_VOLT_BY_BIF] vbif28:%d\n",
				vbif28_raw);
	} else {
		delta_v = abs(gm->rbat.rbat_pull_up_volt - dwVolt);
		if (delta_v == 0)
			delta_v = 1;
#if defined(__LP64__) || defined(_LP64)
		do_div(TRes_temp, delta_v);
#else
		TRes_temp = div_s64(TRes_temp, delta_v);
#endif
	}

#ifdef RBAT_PULL_DOWN_R
	TRes = (TRes_temp * RBAT_PULL_DOWN_R);

#if defined(__LP64__) || defined(_LP64)
	do_div(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));
#else
	TRes_temp = div_s64(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));
#endif

#else
	TRes = TRes_temp;
#endif

	sBaTTMP = BattThermistorConverTemp(gm, (int)TRes);

	dev_info(gm->dev, "[%s] %d %d %d %d\n",
		__func__,
		dwVolt, gm->rbat.rbat_pull_up_r,
		vbif28, volt_cali);
	return sBaTTMP;
}

int force_get_tbat_internal(struct mtk_battery *gm, bool update)
{
	int bat_temperature_volt = 2;
	int bat_temperature_val = 0;
	static int pre_bat_temperature_val = -1;
	int fg_r_value = 0;
	int fg_meter_res_value = 0;
	int fg_current_temp = 0;
	bool fg_current_state = false;
	int bat_temperature_volt_temp = 0;
	int vol_cali = 0;
	static int pre_bat_temperature_volt_temp, pre_bat_temperature_volt;
	static int pre_fg_current_temp;
	static int pre_fg_current_state;
	static int pre_fg_r_value;
	static int pre_bat_temperature_val2;
	static struct timespec pre_time;
	struct timespec ctime, dtime;

	if (update == true || pre_bat_temperature_val == -1) {
		/* Get V_BAT_Temperature */
		gauge_get_property(GAUGE_PROP_BATTERY_TEMPERATURE_ADC,
			&bat_temperature_volt);

		if (bat_temperature_volt != 0) {
			fg_r_value = gm->fg_cust_data.com_r_fg_value;
			if (gm->no_bat_temp_compensate == 0)
				fg_meter_res_value =
				gm->fg_cust_data.com_fg_meter_resistance;
			else
				fg_meter_res_value = 0;

			gauge_get_property(GAUGE_PROP_BATTERY_CURRENT,
				&fg_current_temp);

			if (fg_current_temp > 0)
				fg_current_state = true;

			fg_current_temp = abs(fg_current_temp) / 10;

			if (fg_current_state == true) {
				bat_temperature_volt_temp =
					bat_temperature_volt;
				bat_temperature_volt =
				bat_temperature_volt -
				((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
						/ 10000);
				vol_cali =
					-((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
						/ 10000);
			} else {
				bat_temperature_volt_temp =
					bat_temperature_volt;
				bat_temperature_volt =
				bat_temperature_volt +
				((fg_current_temp *
				(fg_meter_res_value + fg_r_value)) / 10000);
				vol_cali =
					((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
					/ 10000);
			}

			bat_temperature_val =
				BattVoltToTemp(gm,
				bat_temperature_volt,
				vol_cali);
		}

		dev_notice(gm->dev, "[%s] %d,%d,%d,%d,%d,%d r:%d %d %d\n",
			__func__,
			bat_temperature_volt_temp, bat_temperature_volt,
			fg_current_state, fg_current_temp,
			fg_r_value, bat_temperature_val,
			fg_meter_res_value, fg_r_value,
			gm->no_bat_temp_compensate);

		if (pre_bat_temperature_val2 == 0) {
			pre_bat_temperature_volt_temp =
				bat_temperature_volt_temp;
			pre_bat_temperature_volt = bat_temperature_volt;
			pre_fg_current_temp = fg_current_temp;
			pre_fg_current_state = fg_current_state;
			pre_fg_r_value = fg_r_value;
			pre_bat_temperature_val2 = bat_temperature_val;
			get_monotonic_boottime(&pre_time);
		} else {
			get_monotonic_boottime(&ctime);
			dtime = timespec_sub(ctime, pre_time);

			if (((dtime.tv_sec <= 20) &&
				(abs(pre_bat_temperature_val2 -
				bat_temperature_val) >= 5)) ||
				bat_temperature_val >= 58) {
				dev_notice(gm->dev, "[%s][err] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d\n",
					__func__,
					bat_temperature_volt_temp,
					bat_temperature_volt,
					fg_current_state,
					fg_current_temp,
					fg_r_value,
					bat_temperature_val,
					pre_bat_temperature_volt_temp,
					pre_bat_temperature_volt,
					pre_fg_current_state,
					pre_fg_current_temp,
					pre_fg_r_value,
					pre_bat_temperature_val2);
				/*pmic_auxadc_debug(1);*/
				WARN_ON(1);
			}

			pre_bat_temperature_volt_temp =
				bat_temperature_volt_temp;
			pre_bat_temperature_volt = bat_temperature_volt;
			pre_fg_current_temp = fg_current_temp;
			pre_fg_current_state = fg_current_state;
			pre_fg_r_value = fg_r_value;
			pre_bat_temperature_val2 = bat_temperature_val;
			pre_time = ctime;
			dev_notice(gm->dev, "[%s] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d time:%d\n",
				__func__,
				bat_temperature_volt_temp, bat_temperature_volt,
				fg_current_state, fg_current_temp,
				fg_r_value, bat_temperature_val,
				pre_bat_temperature_volt_temp,
				pre_bat_temperature_volt,
				pre_fg_current_state, pre_fg_current_temp,
				pre_fg_r_value,
				pre_bat_temperature_val2, (int)dtime.tv_sec);
		}
	} else {
		bat_temperature_val = pre_bat_temperature_val;
	}

	return bat_temperature_val;
}

int force_get_tbat(struct mtk_battery *gm, bool update)
{
	int bat_temperature_val = 0;

	if (gm->is_probe_done == false)
		return 25;

	if (gm->fixed_bat_tmp != 0xffff)
		return gm->fixed_bat_tmp;

	bat_temperature_val = force_get_tbat_internal(gm, true);

	return bat_temperature_val;
}

int wakeup_fg_algo_cmd(
	struct mtk_battery *gm, unsigned int flow_state, int cmd, int para1)
{

	dev_info(gm->dev, "[%s] %d %d %d\n", __func__, flow_state, cmd, para1);
	if (gm->disableGM30) {
		dev_notice(gm->dev, "FG daemon is disabled\n");
		return -1;
	}
	do_fg_algo(gm, flow_state);
	return 0;
}

int wakeup_fg_algo(struct mtk_battery *gm, unsigned int flow_state)
{
	return wakeup_fg_algo_cmd(gm, flow_state, 0, 0);
}

static int mtk_battery_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct battery_data *bs_data =
		container_of(nb, struct battery_data, battery_nb);

	pr_notice("%s: %ld\n", __func__, event);

	if (event)
		bs_data->bat_status = POWER_SUPPLY_STATUS_CHARGING;
	else
		bs_data->bat_status = POWER_SUPPLY_STATUS_DISCHARGING;

	power_supply_changed(bs_data->psy);

	return NOTIFY_DONE;
}

static int battery_probe(struct platform_device *pdev)
{
	struct power_supply *psy;
	struct mtk_battery *gm;
	static int cnt;
	int ret;

	pr_notice("[%s]\n", __func__);
	psy = power_supply_get_by_name("mtk-gauge");
	if (psy == NULL) {
		cnt++;
		if (cnt >= 5)
			ret = -ENODEV;
		else
			ret = -EPROBE_DEFER;
		pr_notice("%s: can not get gauge, cnt:%d\n", __func__, cnt);
		return ret;
	}

	gm = devm_kzalloc(&pdev->dev, sizeof(*gm), GFP_KERNEL);
	if (!gm)
		return -ENOMEM;

	if (psy != NULL) {
		gm->gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
		gm->gauge->gm = gm;
	} else {
		pr_notice("%s: can not get gauge\n", __func__);
		return -ENODEV;
	}

	dev_set_drvdata(&pdev->dev, gm);
	gm->dev = &pdev->dev;
	mutex_init(&gm->ops_lock);

	battery_service_data_init(gm);
	gm->bs_data.psy =
		power_supply_register(
			&(pdev->dev), &gm->bs_data.psd, &gm->bs_data.psy_cfg);
	if (IS_ERR(gm->bs_data.psy)) {
		pr_notice("[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(gm->bs_data.psy);
		return ret;
	}

	gm->bs_data.edev = extcon_get_edev_by_phandle(&pdev->dev, 0);
	if (IS_ERR(gm->bs_data.edev)) {
		pr_notice("couldn't get extcon device: %ld\n",
			PTR_ERR(gm->bs_data.edev));
		ret = PTR_ERR(gm->bs_data.edev);
		goto err_power_supply;
	}

	gm->bs_data.battery_nb.notifier_call = mtk_battery_notifier;
	ret = devm_extcon_register_notifier(&pdev->dev, gm->bs_data.edev,
				EXTCON_USB, &gm->bs_data.battery_nb);
	if (ret < 0) {
		pr_notice("failed to register notifier for EXTCON_USB\n");
		goto err_power_supply;
	}

	if (extcon_get_state(gm->bs_data.edev, EXTCON_USB)) {
		gm->bs_data.bat_status = POWER_SUPPLY_STATUS_CHARGING;
		power_supply_changed(gm->bs_data.psy);
	}

	pr_notice("[BAT_probe] power_supply_register Battery Success !!\n");
	gm->is_probe_done = true;
	mtk_battery_core_init(pdev);
	pr_notice("[%s]: DONE\n", __func__);
	return 0;

err_power_supply:
	power_supply_unregister(gm->bs_data.psy);

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id mtk_bat_of_match[] = {
	{.compatible = "mediatek,bat_gm30", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_bat_of_match);
#endif

struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};

static struct platform_driver battery_driver_probe = {
	.probe = battery_probe,
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
	 .name = "battery",
#ifdef CONFIG_OF
	 .of_match_table = mtk_bat_of_match,
#endif
	},
};

static int __init battery_init(void)
{
	int ret;

#ifdef CONFIG_OF
	/* register battery_device by DTS */
#else
	ret = platform_device_register(&battery_device);
#endif

	ret = platform_driver_register(&battery_driver_probe);
	pr_notice("[%s] Initialization : DONE\n",
	 __func__);

	return 0;
}

static void __exit battery_exit(void)
{
	platform_driver_unregister(&battery_driver_probe);

}
module_init(battery_init);
module_exit(battery_exit);
MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
