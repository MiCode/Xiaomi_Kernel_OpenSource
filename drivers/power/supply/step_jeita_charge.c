/*
 * step/jeita charge controller
 *
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*	date			author			comment
 *	2021-06-01		chenyichun@xiaomi.com	create
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include "mtk_charger.h"

static void get_index(struct step_jeita_cfg0 *cfg, int fallback_hyst, int forward_hyst, int value, int *index, bool ignore_hyst)
{
	int new_index = 0, i = 0;

	if (value < cfg[0].low_threshold) {
		index[0] = index[1] = 0;
		return;
	}

	if (value > cfg[STEP_JEITA_TUPLE_COUNT - 1].high_threshold)
		new_index = STEP_JEITA_TUPLE_COUNT - 1;

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++) {
		if (is_between(cfg[i].low_threshold, cfg[i].high_threshold, value)) {
			new_index = i;
			break;
		}
	}

	if (ignore_hyst) {
		index[0] = index[1] = new_index;
	} else {
		if (new_index > index[0]) {
			if (value < (cfg[new_index].low_threshold + forward_hyst))
				new_index = index[0];
		} else if (new_index < index[0]) {
			if (value > (cfg[new_index].high_threshold - fallback_hyst))
				new_index = index[0];
		}
		index[1] = index[0];
		index[0] = new_index;
	}

	return;
}

static void monitor_jeita_descent(struct mtk_charger *info)
{
	int current_fcc = 0;

	current_fcc = get_client_vote(info->fcc_votable, JEITA_CHARGE_VOTER);
	if (current_fcc != info->jeita_chg_fcc) {
		if (current_fcc >= info->jeita_chg_fcc + JEITA_FCC_DESCENT_STEP)
			vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, current_fcc - JEITA_FCC_DESCENT_STEP);
		else if (current_fcc >= info->jeita_chg_fcc - JEITA_FCC_DESCENT_STEP)
			vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
		else
			vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, current_fcc + JEITA_FCC_DESCENT_STEP);
	}
}

static void check_bat_ovp(struct mtk_charger *info)
{
	if(info->temp_now >= 480 && info->vbat_now > 4150){
		info->is_bat_ovp = true;
		charger_dev_enable(info->chg1_dev, false);
		vote(info->fv_votable, BAT_OVP_VOTER, true, BAT_OVP_VOLTAGE_HIGH);
	}else if(info->is_bat_ovp && !info->charge_full &&
		(info->temp_now < 470 || info->vbat_now <= 4050)){
		info->is_bat_ovp = false;
		charger_dev_enable(info->chg1_dev, true);
	}

	return;
}

static void handle_jeita_charge(struct mtk_charger *info)
{
	static bool jeita_vbat_low = true;
	int diff_curr_val = 50;

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, false);

	if(!info->is_bat_ovp)
		vote(info->fv_votable, JEITA_CHARGE_VOTER, true, info->jeita_fv_cfg[info->jeita_chg_index[0]].value - 2 * info->diff_fv_val);

	if (jeita_vbat_low) {
		if (info->vbat_now < (info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold + diff_curr_val)) {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
		} else {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
			jeita_vbat_low = false;
		}
	} else {
		if (info->vbat_now < (info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold - diff_curr_val)) {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
			jeita_vbat_low = true;
		} else {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
		}
	}

	return;
}

static void monitor_night_charging(struct mtk_charger *info)
{
	if(info == NULL || !info->fcc_votable)
		return;
	if(info->night_charging && info->soc >=80)
	{
		vote(info->fcc_votable, NIGHT_CHARGING_VOTER, true, 0);
	}
	else if(!info->night_charging || info->soc <=75)
	{
		vote(info->fcc_votable, NIGHT_CHARGING_VOTER, false, 0);
	}
}

static void monitor_thermal_limit(struct mtk_charger *info)
{
	int thermal_level = 0;
	if (info->thermal_level < 0)
		thermal_level = -1 - info->thermal_level;
	else
		thermal_level = info->thermal_level;

	switch(info->usb_type) {
	case POWER_SUPPLY_USB_TYPE_DCP:
		info->thermal_current = info->thermal_limit[0][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[0][thermal_level]);
		break;
	case POWER_SUPPLY_USB_TYPE_PD:
		info->thermal_current = info->thermal_limit[1][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[1][thermal_level]);
		break;
	default:
		chr_err("not support psy_type to check charger parameters");
	}
}

static void monitor_dynamic_mivr(struct mtk_charger *info)
{
	static int ffc_constant_voltage = 4450000;

	if ((info->cycle_count >= info->chg_cycle_count_level1) &&
		(info->cycle_count < info->chg_cycle_count_level2))
			ffc_constant_voltage = info->ffc_cv_1;
	else if ((info->cycle_count >= info->chg_cycle_count_level2) &&
		(info->cycle_count < info->chg_cycle_count_level3))
			ffc_constant_voltage = info->ffc_cv_2;
	else if (info->cycle_count >= info->chg_cycle_count_level3)
			ffc_constant_voltage = info->ffc_cv_3;

	vote(info->fv_votable, MIVR_VOTER, true, ffc_constant_voltage / 1000);

	return;
}

static void check_full_recharge(struct mtk_charger *info)
{
	static int full_count = 0, recharge_count = 0, iterm = 450;
	int fv_effective = get_effective_result(info->fv_votable);
	if (info->charge_full) {
		full_count = 0;
		if (info->vbat_now <= fv_effective - 50)
			recharge_count++;
		else
			recharge_count = 0;

		if (recharge_count >= 15 || (info->soc <= 99 && info->temp_now <= 460)) {
			info->charge_full = false;
			info->recharge = true;
			recharge_count = 0;
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			charger_dev_enable(info->chg1_dev, true);
		}
	} else {
		recharge_count = 0;
		if ((info->soc == 100 && (info->vbat_now >= fv_effective) && (info->current_now <= iterm))
			|| ((info->vbat_now >= fv_effective - 20) && (info->current_now <= iterm + 30) && (info->temp_now >= 480)))
			full_count++;
		else
			full_count = 0;

		if (full_count >= 6) {
			full_count = 0;
			info->charge_full = true;
			if(info->temp_now < 480)
				charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			info->recharge = false;
			charger_dev_enable(info->chg1_dev, false);
		}
	}
}

static void check_charge_data(struct mtk_charger *info)
{
	union power_supply_propval pval = {0,};
	int ret = 0;

	if (!info->battery_psy) {
		info->battery_psy = power_supply_get_by_name("battery");
		return;
	}

	charger_dev_is_enabled(info->chg1_dev, &info->bbc_charge_enable);
	charger_dev_is_charging_done(info->chg1_dev, &info->bbc_charge_done);

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret)
		chr_err("failed to get soc\n");
	else
		info->soc = pval.intval;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret)
		chr_err("failed to get vbat\n");
	else
		info->vbat_now = pval.intval / 1000;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret)
		chr_err("failed to get ibat\n");
	else
		info->current_now = pval.intval / 1000;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret)
		chr_err("failed to get temp\n");
	else
		info->temp_now = pval.intval;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &pval);
	if (ret)
		chr_err("failed to get thermal level\n");
	else
		info->thermal_level = pval.intval;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret)
		chr_err("failed to get cycle count\n");
	else
		info->cycle_count = pval.intval;

}

static void charge_monitor_func(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, charge_monitor_work.work);

	check_charge_data(info);

	check_full_recharge(info);

	check_bat_ovp(info);

	monitor_dynamic_mivr(info);

	monitor_thermal_limit(info);

	handle_jeita_charge(info);

	monitor_night_charging(info);

	monitor_jeita_descent(info);

	schedule_delayed_work(&info->charge_monitor_work, msecs_to_jiffies(FCC_DESCENT_DELAY));
}

void reset_step_jeita_charge(struct mtk_charger *info)
{
	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, true);
	if (info->vbat_now < info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold)
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
	else
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
	vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
}

int step_jeita_init(struct mtk_charger *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int total_length = 0, i = 0, ret = 0;
	if (!np) {
		return -EINVAL;
	}
	total_length = of_property_count_elems_of_size(np, "thermal_limit_dcp", sizeof(u32));
	if (total_length < 0) {
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_dcp", (u32 *)(info->thermal_limit[0]), total_length);
	if (ret) {
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		if (info->thermal_limit[0][i] > MAX_THERMAL_FCC || info->thermal_limit[0][i] < MIN_THERMAL_FCC) {
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[0][i] > info->thermal_limit[0][i - 1]) {
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc2", sizeof(u32));
	if (total_length < 0) {
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc2", (u32 *)(info->thermal_limit[1]), total_length);
	if (ret) {
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		if (info->thermal_limit[1][i] > MAX_THERMAL_FCC || info->thermal_limit[1][i] < MIN_THERMAL_FCC) {
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[1][i] > info->thermal_limit[1][i - 1]) {
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "jeita_fcc_cfg", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "jeita_fcc_cfg", (u32 *)info->jeita_fcc_cfg, total_length);
	if (ret) {
		chr_err("failed to parse jeita_fcc_cfg\n");
		return ret;
	}

	total_length = of_property_count_elems_of_size(np, "jeita_fv_cfg", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "jeita_fv_cfg", (u32 *)info->jeita_fv_cfg, total_length);
	if (ret) {
		chr_err("failed to parse jeita_fv_cfg\n");
		return ret;
	}

	ret = of_property_read_u32(np, "step_fallback_hyst", &info->step_fallback_hyst);
	if (ret) {
		chr_err("failed to parse step_fallback_hyst\n");
		return ret;
	}

	ret = of_property_read_u32(np, "step_forward_hyst", &info->step_forward_hyst);
	if (ret) {
		chr_err("failed to parse step_forward_hyst\n");
		return ret;
	}

	ret = of_property_read_u32(np, "jeita_fallback_hyst", &info->jeita_fallback_hyst);
	if (ret) {
		chr_err("failed to parse jeita_fallback_hyst\n");
		return ret;
	}

	ret = of_property_read_u32(np, "jeita_forward_hyst", &info->jeita_forward_hyst);
	if (ret) {
		chr_err("failed to parse jeita_forward_hyst\n");
		return ret;
	}

	INIT_DELAYED_WORK(&info->charge_monitor_work, charge_monitor_func);

	return ret;
}
