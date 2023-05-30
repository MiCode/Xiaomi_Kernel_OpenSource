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
#include "mtk_charger.h"
#include "adapter_class.h"
#include "bq28z610.h"

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

static void monitor_sw_cv(struct mtk_charger *info)
{
	int ibat = 0;

	if (info->step_chg_index[0] > info->step_chg_index[1] && (info->step_chg_cfg[info->step_chg_index[0]].value != info->step_chg_cfg[info->step_chg_index[1]].value)) {
		info->sw_cv_count = 0;
		info->sw_cv = info->step_chg_cfg[info->step_chg_index[0]].low_threshold + info->step_forward_hyst;
	}

	if (info->sw_cv || info->suspend_recovery) {
		ibat = info->current_now;
		if ((-ibat) <= info->step_chg_fcc) {
			info->sw_cv_count++;
			if (info->sw_cv_count >= SW_CV_COUNT) {
				info->sw_cv = 0;
				info->sw_cv_count = 0;
				vote(info->fcc_votable, STEP_CHARGE_VOTER, true, info->step_chg_fcc);
			}
		} else {
			info->sw_cv_count = 0;
		}
	}
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

static int typec_connect_ntc_set_vbus(struct mtk_charger *info, bool is_on)
{
	struct regulator *vbus = info->vbus_contral;
	int ret = 0, vbus_vol = 3300000;

	/* vbus is optional */
	if (!vbus)
		return 0;

	chr_err("contral vbus turn %s\n", is_on ? "on" : "off");

	if (is_on) {
		ret = regulator_set_voltage(vbus, vbus_vol, vbus_vol);
		if (ret)
			chr_err("vbus regulator set voltage failed\n");

		ret = regulator_enable(vbus);
		if (ret)
			chr_err("vbus regulator enable failed\n");
	} else {
		ret = regulator_disable(vbus);
		if (ret)
			chr_err("vbus regulator disable failed\n");
	}

	return 0;
}
static void charger_set_dpdm_voltage(struct mtk_charger *info, int dp, int dm)
{
	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger\n");
	else {
		chr_err("*** Error : can't find primary charger ***\n");
		return;
	}
	charger_dev_set_dpdm_voltage(info->chg1_dev, dp, dm);
}
static void usbotg_enable_otg(struct mtk_charger *info, bool is_on)
{
	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger\n");
	else {
		chr_err("*** Error : can't find primary charger ***\n");
		return;
	}
	charger_dev_enable_otg_regulator(info->chg1_dev, is_on);
}

static void monitor_usb_otg_burn(struct work_struct *work)
{
	int otg_monitor_delay_time = 5000;
	struct mtk_charger *info = container_of(work, struct mtk_charger, usb_otg_monitor_work.work);
	int type_temp = 0, pmic_vbus = 0;
	usb_get_property(USB_PROP_CONNECTOR_TEMP, &type_temp);
	usb_get_property(USB_PROP_PMIC_VBUS, &pmic_vbus);
	if(type_temp <= 450)
		otg_monitor_delay_time = 5000;
	else if(type_temp > 450 && type_temp <= 550)
		otg_monitor_delay_time = 2000;
	else if(type_temp > 550)
		otg_monitor_delay_time = 1000;
	if(type_temp >= TYPEC_BURN_TEMP && !info->typec_otg_burn)
	{
		info->typec_otg_burn = true;
		usbotg_enable_otg(info, false);
		chr_err("%s:disable otg\n", __func__);
		charger_dev_cp_reset_check(info->cp_master);
		charger_dev_cp_reset_check(info->cp_slave);
	}
	else if((info->typec_otg_burn && type_temp <= (TYPEC_BURN_TEMP - TYPEC_BURN_HYST)) ||(pmic_vbus < 1000 && info->otg_enable && (type_temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST)))
	{
		info->typec_otg_burn = false;
		usbotg_enable_otg(info, true);
		chr_err("%s:enable otg\n", __func__);
	}
	schedule_delayed_work(&info->usb_otg_monitor_work, msecs_to_jiffies(otg_monitor_delay_time));
}

static void monitor_typec_burn(struct mtk_charger *info)
{
	int type_temp = 0, retry_count = 2;
	bool cp_master_enable = false, cp_slave_enable = false;

	usb_get_property(USB_PROP_CONNECTOR_TEMP, &type_temp);

	if (type_temp >= TYPEC_BURN_TEMP && !info->typec_burn_status && !info->otg_enable) {
		info->typec_burn = true;
		while (retry_count) {
			if (info->cp_master && info->cp_slave) {
				charger_dev_is_enabled(info->cp_master, &cp_master_enable);
				charger_dev_is_enabled(info->cp_slave, &cp_slave_enable);
			}
			if (!cp_master_enable && !cp_slave_enable)
				break;
			msleep(80);
			retry_count--;
		}
		adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 1000);
          	msleep(200);
		if (info->real_type == XMUSB350_TYPE_HVCHG)
			charger_set_dpdm_voltage(info, 0, 0);
		else if(info->real_type == XMUSB350_TYPE_HVDCP_2 || info->real_type == XMUSB350_TYPE_HVDCP_3 ||
						info->real_type == XMUSB350_TYPE_HVDCP_35_18 || info->real_type == XMUSB350_TYPE_HVDCP_35_27 ||
						info->real_type == XMUSB350_TYPE_HVDCP_3_18 || info->real_type == XMUSB350_TYPE_HVDCP_3_27)
			charger_dev_select_qc_mode(info->usb350_dev, QC_MODE_QC2_5);
		usb_set_property(USB_PROP_INPUT_SUSPEND, info->typec_burn);
		vote(info->icl_votable, TYPEC_BURN_VOTER, true, 0);
		msleep(1000);
		if (info->mt6368_moscon1_control)
			mtk_set_mt6368_moscon1(info, 1, 1);
		else
			typec_connect_ntc_set_vbus(info, true);
		info->typec_burn_status = true;
	} else if (info->typec_burn && type_temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST && info->typec_burn_status) {
		info->typec_burn = false;
		if (info->mt6368_moscon1_control)
			mtk_set_mt6368_moscon1(info, 0, 0);
		else
			typec_connect_ntc_set_vbus(info, false);
		usb_set_property(USB_PROP_INPUT_SUSPEND, info->typec_burn);
		vote(info->icl_votable, TYPEC_BURN_VOTER, false, 0);
		info->typec_burn_status = false;
	}
}

static void handle_jeita_charge(struct mtk_charger *info)
{
	static bool jeita_vbat_low = true;
	int diff_curr_val = 0;

	if (info->product_name == XAGAPRO)
		diff_curr_val = 100;
	else
		diff_curr_val = 50;

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, false);

	vote(info->fv_votable, JEITA_CHARGE_VOTER, true, info->jeita_fv_cfg[info->jeita_chg_index[0]].value  - info->diff_fv_val);

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

	if (is_between(info->jeita_fcc_cfg[0].low_threshold, info->jeita_fcc_cfg[STEP_JEITA_TUPLE_COUNT - 1].high_threshold, info->temp_now) && !info->input_suspend && !info->typec_burn && info->bms_i2c_error_count < 10)
        chr_err("handle_jeita_charge index = %d,jeita_chg_fcc = %d", info->jeita_chg_index[0], info->jeita_chg_fcc);

	if(info->bms_i2c_error_count >= 10)
	{
		if((info->product_name == XAGA) || (info->product_name == XAGAPRO)){
			vote_override(info->fcc_votable, FG_I2C_VOTER, true, 500);
			vote_override(info->icl_votable, FG_I2C_VOTER, true, 500);
		}else{
			charger_dev_enable(info->chg1_dev, false);
		}
	}
	return;
}

static void handle_step_charge(struct mtk_charger *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->vbat_now, info->step_chg_index, false);

	if (info->step_chg_index[0] == STEP_JEITA_TUPLE_COUNT - 1)
		info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value + 100;
	else
		info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;

	 chr_err("handle_step_charge index = %d", info->step_chg_index[0]);
	return;
}

static void monitor_thermal_limit(struct mtk_charger *info)
{
	int thermal_level = 0;
	int iterm_effective = get_effective_result(info->iterm_votable);
	if (info->thermal_level < 0)
		thermal_level = -1 - info->thermal_level;
	else
	{
		if(info->sic_support)
			thermal_level = (info->thermal_level < 14) ? 0 : info->thermal_level;
		else
			thermal_level = info->thermal_level;
	}

	switch(info->real_type) {
	case XMUSB350_TYPE_DCP:
		info->thermal_current = info->thermal_limit[0][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[0][thermal_level]);
		break;
	case XMUSB350_TYPE_HVDCP:
	case XMUSB350_TYPE_HVDCP_2:
		info->thermal_current = info->thermal_limit[1][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[1][thermal_level]);
		break;
	case XMUSB350_TYPE_HVDCP_3:
	case XMUSB350_TYPE_HVDCP_3_18:
		info->thermal_current = info->thermal_limit[2][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[2][thermal_level]);
		break;
	case XMUSB350_TYPE_HVDCP_3_27:
		info->thermal_current = info->thermal_limit[3][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[3][thermal_level]);
		break;
	case XMUSB350_TYPE_HVDCP_35_18:
	case XMUSB350_TYPE_HVDCP_35_27:
		info->thermal_current = info->thermal_limit[4][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[4][thermal_level]);
		break;
	case XMUSB350_TYPE_PD:
		info->thermal_current = info->thermal_limit[5][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[5][thermal_level]);
		break;
	case XMUSB350_TYPE_HVCHG:
		info->thermal_current = info->thermal_limit[1][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[1][thermal_level]);
		break;
	default:
		chr_err("not support psy_type to check charger parameters");
	}
	if (info->last_thermal_level == 15 && thermal_level < 15) {
		chr_err("[CHARGE_LOOP] disable TE\n");
		charger_dev_enable_termination(info->chg1_dev, false);
		info->disable_te_count = 0;
	}
	if(abs(info->current_now) <= (iterm_effective + 150))
		info->disable_te_count ++;
	else
		info->disable_te_count = 0;
	chr_err("[CHARGE_LOOP] disable_te_count = %d\n", info->disable_te_count);
	if(info->disable_te_count == 5)
	{
		chr_err("[CHARGE_LOOP] enable TE\n");
		charger_dev_enable_termination(info->chg1_dev, true);
	}
	info->last_thermal_level = thermal_level;
}
static int handle_ffc_charge(struct mtk_charger *info)
{
	int ret = 0, val = 0, iterm_ffc = 0, iterm = 0, raw_soc = 0;
	if(info->bms_psy){
		bms_get_property(BMS_PROP_CHARGE_DONE, &val);
		if (val != info->fg_full) {
			info->fg_full = val;
			power_supply_changed(info->psy1);
		}
	} else {
		chr_err("get ti_gauge bms failed\n");
		return 0;
	}
	if ((!info->recharge && info->entry_soc <= info->ffc_high_soc && is_between(info->ffc_low_tbat, info->ffc_high_tbat, info->temp_now) &&
			(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->pd_adapter->verifed) && (info->thermal_level <= 14)) || info->suspend_recovery)
		info->ffc_enable = true;
	else
		info->ffc_enable = false;

	if (info->temp_now >= info->ffc_high_tbat )
		iterm = info->iterm_warm;
	else
		iterm = info->iterm;

	if (is_between(info->ffc_medium_tbat, info->ffc_high_tbat, info->temp_now))
		iterm_ffc = min(info->iterm_ffc_warm, 800);
	else
		iterm_ffc = min(info->iterm_ffc, 800);

	if (!info->gauge_authentic)
		vote(info->fcc_votable, FFC_VOTER, true, 2000);
	else
		vote(info->fcc_votable, FFC_VOTER, true, 22000);

	bms_get_property(BMS_PROP_CAPACITY_RAW, &raw_soc);
	if (info->ffc_enable) {
		vote(info->fv_votable, FFC_VOTER, true, info->fv_ffc - info->diff_fv_val);
		vote(info->iterm_votable, FFC_VOTER, true, iterm_ffc - 100);
		val = true;
		bms_set_property(BMS_PROP_FASTCHARGE_MODE, val);
		val = FG_MONITOR_DELAY_5S;
		bms_set_property(BMS_PROP_MONITOR_DELAY, val);
	} else if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		vote(info->fv_votable, FFC_VOTER, true, info->fv  - info->diff_fv_val);
		vote(info->iterm_votable, FFC_VOTER, true, iterm - 50);
		val = false;
		bms_set_property(BMS_PROP_FASTCHARGE_MODE, val);
		val = FG_MONITOR_DELAY_5S;
		bms_set_property(BMS_PROP_MONITOR_DELAY, val);
	} else {
		vote(info->fv_votable, FFC_VOTER, true, info->fv  - info->diff_fv_val);
		vote(info->iterm_votable, FFC_VOTER, true, iterm - 50);
		val = false;
		bms_set_property(BMS_PROP_FASTCHARGE_MODE, val);
		val = FG_MONITOR_DELAY_30S;
		bms_set_property(BMS_PROP_MONITOR_DELAY, val);
	}

	return ret;
}

static void check_full_recharge(struct mtk_charger *info)
{
	static int full_count = 0, recharge_count = 0, iterm = 0, threshold_mv = 0, real_full = 0;
	int iterm_effective = get_effective_result(info->iterm_votable);
	int fv_effective = get_effective_result(info->fv_votable);
	int rsoc = 0;

	if (info->ffc_enable) {
		if (is_between(info->ffc_medium_tbat, info->ffc_high_tbat, info->temp_now))
			iterm = info->iterm_ffc_warm;
		else
			iterm = info->iterm_ffc;
	} else {
		if (info->temp_now >= info->ffc_high_tbat)
			iterm = info->iterm_warm;
		else
			iterm = info->iterm;
	}

	if (info->charge_full) {
		full_count = 0;
		if (info->vbat_now <= fv_effective - 150)
			recharge_count++;
		else
			recharge_count = 0;

		bms_get_property(BMS_PROP_RSOC, &rsoc);

		if (((recharge_count >= 15) || (rsoc < 98)) && (info->temp_now < 460)) {
			info->charge_full = false;
			if(real_full)
				info->recharge = true;
			recharge_count = 0;
			if(info->bms_i2c_error_count < 10)
				charger_dev_enable(info->chg1_dev, true);
			power_supply_changed(info->psy1);
		}
	} else {
		recharge_count = 0;
		if (info->temp_now > 0)
			threshold_mv = 50;
		else
			threshold_mv = 100;
		if ((info->soc == 100 && (info->vbat_now >= fv_effective - threshold_mv) && (((-info->current_now <= iterm) && info->fg_full) || ((-info->current_now <= iterm_effective + 100) && info->bbc_charge_done)))
                    || ((info->vbat_now >= fv_effective - 30) && (-info->current_now <= iterm) && (info->temp_now >= 481)))
			full_count++;
		else
			full_count = 0;

		if (full_count >= 6) {
			full_count = 0;
			info->charge_full = true;
			if(info->temp_now < 481){
				real_full = 1;
				charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
				power_supply_changed(info->psy1);
			}
			info->recharge = false;
			charger_dev_enable(info->chg1_dev, false);
		}

		if ((info->bbc_charge_done && info->temp_now < 475 && info->temp_now >0) && (info->soc < 100 || (info->soc == 100 && info->bbc_charge_enable && (info->vbat_now < fv_effective - threshold_mv) && !info->charge_full))) {
			charger_dev_enable_powerpath(info->chg1_dev, false);
			charger_dev_enable(info->chg1_dev, false);
			msleep(500);
			charger_dev_enable_powerpath(info->chg1_dev, true);
			charger_dev_enable(info->chg1_dev, true);
		}
	}
}

static void check_charge_data(struct mtk_charger *info)
{
	union power_supply_propval pval = {0,};
	int ret = 0, val = 0;

	if (!info->bms_psy) {
		info->bms_psy = power_supply_get_by_name("bms");
		chr_err("failed to get bms_psy\n");
		return;
	}

	if (!info->battery_psy) {
		info->battery_psy = power_supply_get_by_name("battery");
		chr_err("failed to get battery_psy\n");
		return;
	}

	if (!info->cp_master || !info->cp_slave) {
		info->cp_master = get_charger_by_name("cp_master");
		info->cp_slave = get_charger_by_name("cp_slave");
		chr_err("failed to get cp charger\n");
		return;
	}

	charger_dev_is_enabled(info->chg1_dev, &info->bbc_charge_enable);
	charger_dev_is_charging_done(info->chg1_dev, &info->bbc_charge_done);

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret)
		chr_err("failed to get soc\n");
	else
		info->soc = pval.intval;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret)
		chr_err("failed to get vbat\n");
	else
		info->vbat_now = pval.intval / 1000;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret)
		chr_err("failed to get ibat\n");
	else
		info->current_now = pval.intval / 1000;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret)
		chr_err("failed to get tbat\n");
	else
		info->temp_now = pval.intval;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &pval);
	if (ret)
		chr_err("failed to get thermal level\n");
	else
		info->thermal_level = pval.intval;

	ret = bms_get_property(BMS_PROP_I2C_ERROR_COUNT, &val);
	if (ret)
		chr_err("failed to get i2c error count\n");
	else
		info->bms_i2c_error_count = val;

	ret = bms_get_property(BMS_PROP_AUTHENTIC, &val);
	if (ret)
		chr_err("failed to get gauge authentic\n");
	else
		info->gauge_authentic = val;

	if (info->input_suspend || info->typec_burn)
		chr_err("[CHARGE_LOOP] input_suspend = %d, typec_burn = %d\n", info->input_suspend, info->typec_burn);
}

static void monitor_night_charging(struct mtk_charger *info)
{
	if(info == NULL || !info->fcc_votable)
		return;
	if(info->night_charging && info->soc >=80 && !info->night_charge_enable)
	{
		info->night_charge_enable = true;
		charger_dev_enable(info->chg1_dev, false);
	}
	else if((!info->night_charging || info->soc <=75) && info->night_charge_enable)
	{
		info->night_charge_enable = false;
		charger_dev_enable(info->chg1_dev, true);
	}
}

static void charge_monitor_func(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, charge_monitor_work.work);

	check_charge_data(info);

	handle_ffc_charge(info);

	check_full_recharge(info);

	monitor_thermal_limit(info);

	handle_step_charge(info);

	handle_jeita_charge(info);

	monitor_night_charging(info);

	monitor_sw_cv(info);

	monitor_jeita_descent(info);

	monitor_typec_burn(info);

	schedule_delayed_work(&info->charge_monitor_work, msecs_to_jiffies(FCC_DESCENT_DELAY));
}

static int debug_cycle_count = 0;
module_param_named(debug_cycle_count, debug_cycle_count, int, 0600);

static int parse_step_charge_config(struct mtk_charger *info, bool farce_update)
{
	struct device_node *np = info->pdev->dev.of_node;
	union power_supply_propval pval = {0,};
	static bool low_cycle = true;
	bool update = false;
	int total_length = 0, ret = 0;

	if (!info->bms_psy)
		info->bms_psy = power_supply_get_by_name("bms");

	if (info->bms_psy) {
		ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret)
			pval.intval = 0;
	} else {
		pval.intval = 0;
	}

	if (debug_cycle_count > 0)
		info->cycle_count = debug_cycle_count;
	else
		info->cycle_count = pval.intval;

	if (low_cycle) {
		if (info->cycle_count > 100) {
			low_cycle = false;
			update = true;
		}
	} else {
		if (info->cycle_count <= 100) {
			low_cycle = true;
			update = true;
		}
	}

	if (update || farce_update) {
		if (low_cycle) {
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
	}

	return ret;
}

void reset_step_jeita_charge(struct mtk_charger *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->vbat_now, info->step_chg_index, true);
	info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;
	vote(info->fcc_votable, STEP_CHARGE_VOTER, true, info->step_chg_fcc);

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, true);
	if (info->vbat_now < info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold)
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
	else
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
	vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
	chr_err(" fcc:%d,low_value:%d,high_value:%d",info->jeita_chg_fcc,
	info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value, info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value);

	parse_step_charge_config(info, false);
}

int step_jeita_init(struct mtk_charger *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int total_length = 0, i = 0, ret = 0;
	bool product_name = 0;
	if (!np) {
		chr_err("no device node\n");
		return -EINVAL;
	}
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

	info->cycle_count = 0;
	ret = parse_step_charge_config(info, true);
	if (ret) {
		chr_err("failed to parse step_chg_cfg\n");
		return ret;
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

	info->vbus_contral = devm_regulator_get(dev, "vbus_control");
	if (IS_ERR(info->vbus_contral))
		chr_err("failed to get vbus contral\n");
	info->product_name = MATISSE;
	info->mt6368_moscon1_control = of_property_read_bool(np, "mt6368_moscon1_control");
	if (info->mt6368_moscon1_control)
	{
		info->product_name = RUBENS;
		chr_err("successed to parse mt6368_moscon1_control\n");
	}
	product_name = of_property_read_bool(np, "xaga_product");
	if (product_name)
	{
		info->product_name = XAGA;
		chr_err("successed to parse xaga_product\n");
	}
	product_name = of_property_read_bool(np, "xagapro_product");
	if (product_name)
	{
		info->product_name = XAGAPRO;
		chr_err("successed to parse xagapro_product\n");
	}
	info->sic_support =  of_property_read_bool(np, "sic_support");
	if (!info->sic_support)
		chr_err("failed to parse sic_support\n");
	INIT_DELAYED_WORK(&info->charge_monitor_work, charge_monitor_func);
	INIT_DELAYED_WORK(&info->usb_otg_monitor_work, monitor_usb_otg_burn);

	return ret;
}
