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
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/power_supply.h>
#include "mtk_charger_intf.h"
#include <mtk_intf.h>

enum product_name {
	PISSARRO,
	PISSARROPRO,
};

static int product_name = PISSARRO;

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

static void monitor_sw_cv(struct charger_manager *info)
{
	union power_supply_propval pval = {0,};
	int ibat = 0;
	bool is_fast_type = false;

	if (info->qc3_type || info->psy_type == POWER_SUPPLY_TYPE_USB_PD)
		is_fast_type = true;
	else
		is_fast_type = false;

	if (info->step_chg_index[0] > info->step_chg_index[1] && (info->step_chg_cfg[info->step_chg_index[0]].value != info->step_chg_cfg[info->step_chg_index[1]].value)) {
		info->sw_cv_count = 0;
		info->sw_cv = info->step_chg_cfg[info->step_chg_index[0]].low_threshold + info->step_forward_hyst;
	}

	if (info->sw_cv) {
		power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		ibat = pval.intval / 1000;
		if ((is_fast_type && (-ibat) <= info->step_chg_fcc && (-ibat) >= 400) || (!is_fast_type && (-ibat) <= info->step_chg_fcc)) {
			info->sw_cv_count++;
			if (info->sw_cv_count >= SW_CV_COUNT) {
				info->sw_cv = 0;
				info->sw_cv_count = 0;
				vote(info->bbc_fcc_votable, STEP_CHARGE_VOTER, true, info->step_chg_fcc);
			}
		} else {
			info->sw_cv_count = 0;
		}
	}
}

static void monitor_jeita_descent(struct charger_manager *info)
{
	int current_fcc = 0;

	current_fcc = get_client_vote(info->bbc_fcc_votable, JEITA_CHARGE_VOTER);
	if (current_fcc != info->jeita_chg_fcc) {
		if (current_fcc >= info->jeita_chg_fcc + JEITA_FCC_DESCENT_STEP)
			vote(info->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, current_fcc - JEITA_FCC_DESCENT_STEP);
		else if (current_fcc >= info->jeita_chg_fcc - JEITA_FCC_DESCENT_STEP)
			vote(info->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
		else
			vote(info->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, current_fcc + JEITA_FCC_DESCENT_STEP);
	}
}

static void monitor_typec_burn(struct charger_manager *info)
{
	union power_supply_propval pval = {0,};
	int type_temp = 0, retry_count = 10;
	bool cp_master_enable = false, cp_slave_enable = false;

	if (!gpio_is_valid(info->vbus_control_gpio))
		return;

	power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_CONNECTOR_TEMP, &pval);
	type_temp = pval.intval;

	if (type_temp >= TYPEC_BURN_TEMP) {
		if (!info->typec_burn_wakelock.active)
			__pm_stay_awake(&info->typec_burn_wakelock);
		info->typec_burn = true;
		while (retry_count) {
			charger_dev_is_enabled(info->cp_master, &cp_master_enable);
			charger_dev_is_enabled(info->cp_slave, &cp_slave_enable);
			if (!cp_master_enable && !cp_slave_enable)
				break;
			msleep(80);
			retry_count--;
		}
		vote(info->bbc_icl_votable, TYPEC_BURN_VOTER, true, 0);
		vote(info->bbc_en_votable, BBC_ENABLE_VOTER, false, 0);
		gpio_direction_output(info->vbus_control_gpio, 1);
	} else if (info->typec_burn && type_temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST) {
		info->typec_burn = false;
		gpio_direction_output(info->vbus_control_gpio, 0);
		vote(info->bbc_icl_votable, TYPEC_BURN_VOTER, false, 0);
		vote(info->bbc_en_votable, BBC_ENABLE_VOTER, true, 0);
		__pm_relax(&info->typec_burn_wakelock);
	}
}

static void handle_jeita_charge(struct charger_manager *info)
{
	static bool input_suspend = false;
	static bool out_jeita = false;

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->tbat, info->jeita_chg_index, false);

	vote(info->bbc_fv_votable, JEITA_CHARGE_VOTER, true, info->jeita_fv_cfg[info->jeita_chg_index[0]].value);

	if (info->vbat < info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold)
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
	else
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;

	if (info->input_suspend != input_suspend) {
		input_suspend = info->input_suspend;
		charger_dev_enable_powerpath(info->chg1_dev, !input_suspend);
		power_supply_changed(info->usb_psy);
	}

	if (is_between(info->jeita_fcc_cfg[0].low_threshold, info->jeita_fcc_cfg[STEP_JEITA_TUPLE_COUNT - 1].high_threshold, info->tbat) && !info->typec_burn
		&& info->bms_i2c_error_count < 10) {
		out_jeita = false;
		vote(info->bbc_en_votable, BBC_ENABLE_VOTER, true, 0);
	} else {
		if (product_name == PISSARROPRO && !out_jeita) {
			if (info->psy_type == POWER_SUPPLY_TYPE_USB_HVDCP) {
				charger_dev_enable_powerpath(info->chg1_dev, false);
				msleep(500);
				charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_5);
				charger_dev_enable_powerpath(info->chg1_dev, true);
			} else if (info->psy_type == POWER_SUPPLY_TYPE_USB_PD) {
				vote(info->bbc_fcc_votable, CV_WA_VOTER, true, 500);
				vote(info->bbc_icl_votable, CV_WA_VOTER, true, 500);
				charger_dev_enable_powerpath(info->chg1_dev, false);
				msleep(500);
				adapter_set_cap(5000, 1000);
				vote(info->bbc_fcc_votable, CV_WA_VOTER, false, 0);
				vote(info->bbc_icl_votable, CV_WA_VOTER, false, 0);
				charger_dev_enable_powerpath(info->chg1_dev, true);
			}
			msleep(100);
		}
		out_jeita = true;
		vote(info->bbc_en_votable, BBC_ENABLE_VOTER, false, 0);
	}

	return;
}

static void handle_step_charge(struct charger_manager *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->vbat, info->step_chg_index, false);

	if (info->step_chg_index[0] == STEP_JEITA_TUPLE_COUNT - 1)
		info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value + 100;
	else
		info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;

	return;
}

static void monitor_thermal_limit(struct charger_manager *info)
{
	int thermal_level = 0;

	if (info->thermal_level < 0) {
		thermal_level = -1 - info->thermal_level;
		if (info->thermal_limit_fcc)
			info->thermal_limit[5][thermal_level] = info->thermal_limit_fcc;
	} else {
		thermal_level = info->sic_support ? 0 : info->thermal_level;
	}

	switch(info->psy_type) {
	case POWER_SUPPLY_TYPE_USB_DCP:
		vote(info->bbc_fcc_votable, THERMAL_VOTER, true, info->thermal_limit[0][thermal_level]);
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		vote(info->bbc_fcc_votable, THERMAL_VOTER, true, info->thermal_limit[1][thermal_level]);
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS:
		switch(info->qc3_type) {
		case HVDCP3_18:
			vote(info->bbc_fcc_votable, THERMAL_VOTER, true, info->thermal_limit[2][thermal_level]);
			break;
		case HVDCP3_27:
			vote(info->bbc_fcc_votable, THERMAL_VOTER, true, info->thermal_limit[3][thermal_level]);
			break;
		case HVDCP35_18:
		case HVDCP35_27:
			vote(info->bbc_fcc_votable, THERMAL_VOTER, true, info->thermal_limit[4][thermal_level]);
			break;
		default:
			chr_err("not support qc3_type to check charger parameters");
		}
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		vote(info->bbc_fcc_votable, THERMAL_VOTER, true, info->thermal_limit[5][thermal_level]);
		break;
	default:
		break;
	}
}

static void charge_monitor_func(struct work_struct *work)
{
	struct charger_manager *info = container_of(work, struct charger_manager, charge_monitor_work.work);

	monitor_thermal_limit(info);

	handle_step_charge(info);

	handle_jeita_charge(info);

	monitor_sw_cv(info);

	monitor_jeita_descent(info);

	monitor_typec_burn(info);

	schedule_delayed_work(&info->charge_monitor_work, msecs_to_jiffies(FCC_DESCENT_DELAY));
}

static int parse_cycle_count_step_chg_cfg(struct charger_manager *info)
{
	int total_length = 0, i = 0, ret = 0;
	struct device_node *np = info->pdev->dev.of_node;

	if (!np) {
		chr_err("no device node\n");
		return -EINVAL;
	}

	if (info->cycle_count_status == CYCLE_COUNT_LOW) {
		total_length = of_property_count_elems_of_size(np, "step_chg_cfg_low_cycle", sizeof(u32));
		if (total_length < 0) {
			chr_err("failed to read total_length of config\n");
			return -EINVAL;
		}
		ret = of_property_read_u32_array(np, "step_chg_cfg_low_cycle", (u32 *)info->step_chg_cfg, total_length);
		if (ret) {
			chr_err("failed to parse step_chg_cfg_low_cycle\n");
			return ret;
		}
	} else if (info->cycle_count_status == CYCLE_COUNT_NORMAL) {
		total_length = of_property_count_elems_of_size(np, "step_chg_cfg_normal_cycle", sizeof(u32));
		if (total_length < 0) {
			chr_err("failed to read total_length of config\n");
			return -EINVAL;
		}
		ret = of_property_read_u32_array(np, "step_chg_cfg_normal_cycle", (u32 *)info->step_chg_cfg, total_length);
		if (ret) {
			chr_err("failed to parse step_chg_cfg_normal_cycle\n");
			return ret;
		}
	} else {
		total_length = of_property_count_elems_of_size(np, "step_chg_cfg_high_cycle", sizeof(u32));
		if (total_length < 0) {
			chr_err("failed to read total_length of config\n");
			return -EINVAL;
		}
		ret = of_property_read_u32_array(np, "step_chg_cfg_high_cycle", (u32 *)info->step_chg_cfg, total_length);
		if (ret) {
			chr_err("failed to parse step_chg_cfg_high_cycle\n");
			return ret;
		}
	}

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
		chr_info("STEP %d %d %d\n", info->step_chg_cfg[i].low_threshold, info->step_chg_cfg[i].high_threshold, info->step_chg_cfg[i].value);

	return ret;
}

static void check_cycle_count_status(struct charger_manager *info, bool farce_update)
{
	union power_supply_propval pval = {0,};
	bool update = false;
	int ret = 0;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret && !farce_update) {
		chr_err("failed to get cycle_count\n");
		return;
	}

	info->cycle_count = pval.intval;
	if (info->cycle_count <= 50) {
		if (info->cycle_count_status != CYCLE_COUNT_LOW) {
			info->cycle_count_status = CYCLE_COUNT_LOW;
			update = true;
		}
	} else if (info->cycle_count <= 150) {
		if (info->cycle_count_status != CYCLE_COUNT_NORMAL) {
			info->cycle_count_status = CYCLE_COUNT_NORMAL;
			update = true;
		}
	} else {
		if (info->cycle_count_status != CYCLE_COUNT_HIGH) {
			info->cycle_count_status = CYCLE_COUNT_HIGH;
			update = true;
		}
	}

	if (update || farce_update)
		parse_cycle_count_step_chg_cfg(info);

	return;
}

void reset_mi_charge_alg(struct charger_manager *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->vbat, info->step_chg_index, true);
	info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;
	vote(info->bbc_fcc_votable, STEP_CHARGE_VOTER, true, info->step_chg_fcc);

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->tbat, info->jeita_chg_index, true);
	if (info->vbat < info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold)
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
	else
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
	vote(info->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);

	if (product_name == PISSARROPRO && info->charger_status == CHARGER_PLUGOUT)
		check_cycle_count_status(info, false);
}

int step_jeita_init(struct charger_manager *info, struct device *dev, int para)
{
	struct device_node *np = dev->of_node;
	int total_length = 0, i = 0, ret = 0;
	info->cycle_count_status = CYCLE_COUNT_LOW;
	product_name = para;

	if (!np) {
		chr_err("no device node\n");
		return -EINVAL;
	}

	info->vbus_control_gpio = of_get_named_gpio(np, "vbus_control_gpio", 0);
	if (!gpio_is_valid(info->vbus_control_gpio))
		chr_err("failed to parse vbus_control_gpio\n");

	total_length = of_property_count_elems_of_size(np, "thermal_limit_dcp", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_dcp", (u32 *)(info->thermal_limit[0]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_dcp\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_dcp %d\n", info->thermal_limit[0][i]);
		if (info->thermal_limit[0][i] > MAX_THERMAL_FCC || info->thermal_limit[0][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_dcp over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[0][i] > info->thermal_limit[0][i - 1]) {
				chr_err("thermal_limit_dcp order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc2", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc2", (u32 *)(info->thermal_limit[1]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_qc2\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_qc2 %d\n", info->thermal_limit[1][i]);
		if (info->thermal_limit[1][i] > MAX_THERMAL_FCC || info->thermal_limit[1][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_qc2 over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[1][i] > info->thermal_limit[1][i - 1]) {
				chr_err("thermal_limit_qc2 order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc3_18w", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc3_18w", (u32 *)(info->thermal_limit[2]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_qc3_18w\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_qc3_18w %d\n", info->thermal_limit[2][i]);
		if (info->thermal_limit[2][i] > MAX_THERMAL_FCC || info->thermal_limit[2][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_qc3_18w over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[2][i] > info->thermal_limit[2][i - 1]) {
				chr_err("thermal_limit_qc3_18w order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc3_27w", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc3_27w", (u32 *)(info->thermal_limit[3]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_qc3_27w\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_qc3_27w %d\n", info->thermal_limit[3][i]);
		if (info->thermal_limit[3][i] > MAX_THERMAL_FCC || info->thermal_limit[3][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_qc3_27w over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[3][i] > info->thermal_limit[3][i - 1]) {
				chr_err("thermal_limit_qc3_27w order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc35", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc35", (u32 *)(info->thermal_limit[4]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_qc35\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_qc35 %d\n", info->thermal_limit[4][i]);
		if (info->thermal_limit[4][i] > MAX_THERMAL_FCC || info->thermal_limit[4][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_qc35 over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[4][i] > info->thermal_limit[4][i - 1]) {
				chr_err("thermal_limit_qc35 order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_pd", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_pd", (u32 *)(info->thermal_limit[5]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_pd\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_pd %d\n", info->thermal_limit[5][i]);
		if (info->thermal_limit[5][i] > MAX_THERMAL_FCC || info->thermal_limit[5][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_pd over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[5][i] > info->thermal_limit[5][i - 1]) {
				chr_err("thermal_limit_pd order error\n");
				return -1;
			}
		}
	}

	if (product_name == PISSARRO) {
		total_length = of_property_count_elems_of_size(np, "step_chg_cfg", sizeof(u32));
		if (total_length < 0) {
			chr_err("failed to read total_length of config\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_array(np, "step_chg_cfg", (u32 *)info->step_chg_cfg, total_length);
		if (ret) {
			chr_err("failed to parse step_chg_cfg\n");
			return ret;
		}

		for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
			chr_info("STEP %d %d %d\n", info->step_chg_cfg[i].low_threshold, info->step_chg_cfg[i].high_threshold, info->step_chg_cfg[i].value);
	} else {
		check_cycle_count_status(info, true);
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

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
		chr_info("JEITA_FCC %d %d %d %d %d\n", info->jeita_fcc_cfg[i].low_threshold, info->jeita_fcc_cfg[i].high_threshold, info->jeita_fcc_cfg[i].extra_threshold, info->jeita_fcc_cfg[i].low_value, info->jeita_fcc_cfg[i].high_value);

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

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
		chr_info("JEITA_FV %d %d %d\n", info->jeita_fv_cfg[i].low_threshold, info->jeita_fv_cfg[i].high_threshold, info->jeita_fv_cfg[i].value);

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
