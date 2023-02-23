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
	UNKNOW,
	RUBY,
	RUBYPRO,
	RUBYPLUS,
};

static int product_name = UNKNOW;

static void get_index(struct step_jeita_cfg0 *cfg, int fallback_hyst, int forward_hyst, int step_jeita_tuple_count, int value, int *index, bool ignore_hyst)
{
	int new_index = 0, i = 0;

	if (value < cfg[0].low_threshold) {
		index[0] = index[1] = 0;
		return;
	}

	if (value > cfg[step_jeita_tuple_count - 1].high_threshold)
		new_index = step_jeita_tuple_count - 1;

	for (i = 0; i < step_jeita_tuple_count; i++) {
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

static void monitor_usb_otg_burn(struct work_struct *work)
{
	struct charger_manager *info = container_of(work, struct charger_manager, usb_otg_monitor_work.work);
	int otg_monitor_delay_time = 5000;
	int type_temp = 0, pmic_vbus = 0;
	union power_supply_propval pval = {0,};

	power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_CONNECTOR_TEMP, &pval);
	type_temp = pval.intval;
	power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_PMIC_VBUS, &pval);
	pmic_vbus = pval.intval;

	if(type_temp <= 450)
		otg_monitor_delay_time = 5000;
	else if(type_temp > 450 && type_temp <= 550)
		otg_monitor_delay_time = 2000;
	else if(type_temp > 550)
		otg_monitor_delay_time = 1000;

	chr_err("%s: typec_temp =%d otg_monitor_delay_time = %d\n", __func__, type_temp, otg_monitor_delay_time);

	if (type_temp >= TYPEC_BURN_TEMP && !info->typec_otg_burn) {
		info->typec_otg_burn = true;
		charger_dev_enable_otg(info->chg1_dev, false);
		chr_err("%s: disable otg\n", __func__);
		charger_dev_cp_reset_check(info->cp_master);
		charger_dev_cp_reset_check(info->cp_slave);
	} else if((info->typec_otg_burn && type_temp <= (TYPEC_BURN_TEMP - TYPEC_BURN_HYST)) ||(pmic_vbus < 1000 && info->otg_enable && (type_temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST))) {
		info->typec_otg_burn = false;
		charger_dev_enable_otg(info->chg1_dev, true);
		chr_err("%s: enable otg\n", __func__);
	}
	schedule_delayed_work(&info->usb_otg_monitor_work, msecs_to_jiffies(otg_monitor_delay_time));
}

static void monitor_typec_burn(struct charger_manager *info)
{
	union power_supply_propval pval = {0,};
	int type_temp = 0, retry_count = 10, real_type = 0;
	bool cp_master_enable = false, cp_slave_enable = false;

	if (!gpio_is_valid(info->vbus_control_gpio) && product_name == RUBYPRO)
		return;

	power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_CONNECTOR_TEMP, &pval);
	type_temp = pval.intval;

	power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &pval);
	real_type = pval.intval;

	if ((product_name == RUBYPLUS || product_name == RUBY) && type_temp <= TYPEC_BURN_TEMP && type_temp > TYPEC_BURN_REPORT_TEMP && info->typec_burn_flag != 1) {
		power_supply_changed(info->usb_psy);
		info->typec_burn_flag = 1;
          	chr_info("typec burn temp = %d\n",type_temp);
	} else if ((product_name == RUBYPLUS || product_name == RUBY) && type_temp > TYPEC_BURN_TEMP && info->typec_burn_flag != 2) {
		if (!info->typec_burn_wakelock->active)
			__pm_stay_awake(info->typec_burn_wakelock);
		info->typec_burn = true;
		while (retry_count) {
			charger_dev_is_enabled(info->cp_master, &cp_master_enable);
			charger_dev_is_enabled(info->cp_slave, &cp_slave_enable);
			if (!cp_master_enable && !cp_slave_enable)
				break;
			msleep(80);
			retry_count--;
		}
		adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 1000);
		msleep(200);
		if(real_type == POWER_SUPPLY_TYPE_USB_HVDCP)
			charger_dev_set_dpdm_voltage(info->chg1_dev, 0);
		vote(info->bbc_suspend_votable, TYPEC_BURN_VOTER, true, 1);
		vote(info->bbc_icl_votable, TYPEC_BURN_VOTER, true, 0);
		vote(info->bbc_en_votable, TYPEC_BURN_VOTER, true, 0);
		power_supply_changed(info->usb_psy);
		info->typec_burn_flag = 2;
          	chr_info("typec burn temp = %d\n",type_temp);
	} else if ((product_name == RUBYPLUS || product_name == RUBY) && type_temp < TYPEC_BURN_LOW_TEMP && (info->typec_burn || info->typec_burn_flag == 1) && info->typec_burn_flag != 3) {
		info->typec_burn = false;
		power_supply_changed(info->usb_psy);
		info->typec_burn_flag = 3;
          	chr_info("typec burn temp = %d\n",type_temp);
		vote(info->bbc_icl_votable, TYPEC_BURN_VOTER, false, 0);
		vote(info->bbc_en_votable, TYPEC_BURN_VOTER, true, 1);
		vote(info->bbc_suspend_votable, TYPEC_BURN_VOTER, false, 0);
		__pm_relax(info->typec_burn_wakelock);
	}

	if (product_name == RUBYPRO && type_temp >= TYPEC_BURN_TEMP && info->typec_burn_flag != 4) {
		if (!info->typec_burn_wakelock->active)
			__pm_stay_awake(info->typec_burn_wakelock);
		info->typec_burn = true;
		while (retry_count) {
			charger_dev_is_enabled(info->cp_master, &cp_master_enable);
			charger_dev_is_enabled(info->cp_slave, &cp_slave_enable);
			if (!cp_master_enable && !cp_slave_enable)
				break;
			msleep(80);
			retry_count--;
		}

		adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 1000);
		msleep(200);
		if(real_type == POWER_SUPPLY_TYPE_USB_HVDCP)
			charger_dev_set_dpdm_voltage(info->chg1_dev, 0);

		vote(info->bbc_icl_votable, TYPEC_BURN_VOTER, true, 0);
		vote(info->bbc_en_votable, TYPEC_BURN_VOTER, true, 0);
		gpio_direction_output(info->vbus_control_gpio, 1);
		info->typec_burn_flag = 4;
          	chr_info("typec burn temp = %d\n",type_temp);
	} else if (info->typec_burn && product_name == RUBYPRO && type_temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST && info->typec_burn_flag != 5) {
		info->typec_burn = false;
		gpio_direction_output(info->vbus_control_gpio, 0);
		info->typec_burn_flag = 5;
          	chr_info("typec burn temp = %d\n",type_temp);
		vote(info->bbc_icl_votable, TYPEC_BURN_VOTER, false, 0);
		vote(info->bbc_en_votable, TYPEC_BURN_VOTER, true, 1);
		__pm_relax(info->typec_burn_wakelock);
	}
}

static void handle_jeita_charge(struct charger_manager *info)
{
	static bool out_jeita = false;
	static bool jeita_vbat_low = true;

	/*The high temperature drops to 48 degrees to stop charging, need to remove compensation, temp drop to 46 set fv*/
	if (info->tbat > 459) {
		info->jeita_fallback_hyst = 10;
		if (product_name == RUBYPLUS)
		      info->jeita_forward_hyst = 9;
		else
		      info->jeita_forward_hyst = 4;
	} else {
		info->jeita_fallback_hyst = 5;
		info->jeita_forward_hyst = 5;
	}

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->step_jeita_tuple_count, info->tbat, info->jeita_chg_index, false);

	if (jeita_vbat_low) {
		if (info->vbat < (info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold + (product_name == RUBY ? 50 : 100))) {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
		} else {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
			jeita_vbat_low = false;
		}
	} else {
		if (info->vbat < (info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold - (product_name == RUBY ? 50 : 100))) {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
			jeita_vbat_low = true;
		} else {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
		}
	}

	if (is_between(info->jeita_fcc_cfg[0].low_threshold, info->jeita_fcc_cfg[info->step_jeita_tuple_count - 1].high_threshold, info->tbat) && !info->typec_burn
		 && !info->charge_full) {
		out_jeita = false;
		if((product_name == RUBY || product_name == RUBYPRO) && info->jeita_fv_cfg[info->jeita_chg_index[0]].value == 4100 && info->vbat > 4100){
			vote(info->bbc_en_votable, BBC_ENABLE_VOTER, true, 0);
		} else {
			vote(info->bbc_en_votable, BBC_ENABLE_VOTER, true, 1);
		}
	} else {
		if (product_name == RUBY && !out_jeita && info->vbus >= 6000) {
			if (info->psy_type == POWER_SUPPLY_TYPE_USB_HVDCP) {
				vote(info->bbc_suspend_votable, CV_WA_VOTER, true, 1);
				msleep(500);
				if (product_name == RUBYPLUS)
					charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_5);
				else
					charger_dev_set_dpdm_voltage(info->chg1_dev, 0);
				vote(info->bbc_suspend_votable, CV_WA_VOTER, false, 0);
			} else if (info->psy_type == POWER_SUPPLY_TYPE_USB_PD) {
				vote(info->bbc_fcc_votable, CV_WA_VOTER, true, 500);
				vote(info->bbc_icl_votable, CV_WA_VOTER, true, 500);
				vote(info->bbc_suspend_votable, CV_WA_VOTER, true, 1);
				msleep(500);
				adapter_set_cap(5000, 1000);
				vote(info->bbc_fcc_votable, CV_WA_VOTER, false, 0);
				vote(info->bbc_icl_votable, CV_WA_VOTER, false, 0);
				vote(info->bbc_suspend_votable, CV_WA_VOTER, false, 0);
			}
			msleep(100);
		}
		out_jeita = true;
		vote(info->bbc_en_votable, BBC_ENABLE_VOTER, true, 0);
	}

	vote(info->bbc_fv_votable, JEITA_CHARGE_VOTER, true, info->jeita_fv_cfg[info->jeita_chg_index[0]].value - (product_name != RUBYPLUS ? info->diff_fv_val : 2 * info->diff_fv_val));

	return;
}

static void handle_step_charge(struct charger_manager *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->step_jeita_tuple_count, info->vbat, info->step_chg_index, false);

	if (info->step_chg_index[0] == info->step_jeita_tuple_count - 1)
		info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value + 100;
	else
		info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;

	info->step_chg_fv = info->step_chg_cfg[info->step_chg_index[0]].high_threshold + info->step_forward_hyst;
	return;
}

static int handle_ffc_charge(struct charger_manager *info)
{
	int ret = 0, iterm_ffc = 0;
	union power_supply_propval pval = {0,};

	if (!info->charge_full &&!info->recharge && info->entry_soc <= info->ffc_high_soc && (product_name == RUBYPLUS ? is_between(MIN_FFC_JEITA_CHG_INDEX, MAX_FFC_JEITA_CHG_INDEX, info->jeita_chg_index[0]) : info->jeita_chg_index[0] == MIN_FFC_JEITA_CHG_INDEX) &&
		(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->apdo_max >= 33) && (info->thermal_level <= 14))
		info->ffc_enable = true;
	else
		info->ffc_enable = false;

	if (product_name == RUBYPRO && gpio_is_valid(info->vbus_control_gpio) && info->vbus > 3600 && info->soc > info->ffc_high_soc)
		info->ffc_enable = false;

	if (is_between(info->ffc_medium_tbat, info->ffc_high_tbat, info->tbat))
		iterm_ffc = info->iterm_ffc_warm;
	else
		iterm_ffc = info->iterm_ffc;

	info->iterm_effective = info->ffc_enable ? iterm_ffc : info->iterm;
	info->iterm_effective = max(info->iterm_effective - info->full_wa_iterm, 100);

	info->fv_effective = (info->cycle_count > 100) ? info->fv_ffc_large_cycle : info->fv_ffc;
	info->fv_effective = (info->ffc_enable ? info->fv_effective : info->fv) - (product_name != RUBYPLUS ? info->diff_fv_val : 2 * info->diff_fv_val);

	pval.intval = info->ffc_enable ? 1 : 0;
	vote(info->bbc_fv_votable, FFC_VOTER, true, info->fv_effective);
	vote(info->bbc_iterm_votable, FFC_VOTER, true, info->iterm_effective);
	power_supply_set_property(info->bms_psy, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);

	return ret;
}

static void check_full_recharge(struct charger_manager *info)
{
	static int full_count = 0, recharge_count = 0, full_wa_count = 0, threshold_ma = 0, threshold_mv = 0;
	int bbc_full = (product_name == RUBYPLUS) ? MP2762_CHARGE_STATUS_DONE : MT6360_CHARGE_STATUS_DONE;
	int iterm = 0;
	int ret = 0;
	union power_supply_propval pval = {0,};
	int report_full_rsoc = 0;
	int fv_result = 0;
	bool cp_master_enable = false;

	if (info->recheck_count < 2 && product_name == RUBYPLUS) {
		return;
	}

	if (info->ffc_enable) {
		if (is_between(info->ffc_medium_tbat, info->ffc_high_tbat, info->tbat))
			iterm = info->iterm_ffc_warm;
		else
			iterm = info->iterm_ffc;
	} else
		iterm = info->iterm;

	chr_err("%s: ffc_enable = %d, diff_fv_val = %d, iterm = %d, iterm_effective = %d, fv_effective = %d, full_count = %d, recharge_count = %d", __func__, info->ffc_enable, info->diff_fv_val, iterm, info->iterm_effective, info->fv_effective, full_count, recharge_count);

	if (info->charge_full) {
		full_count = 0;
		full_wa_count = 0;

		if(product_name == RUBYPLUS) {
			ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CAPACITY_RAW, &pval);
			if (ret)
			      chr_err("failed to get rawsoc\n");
			else
			      info->rawsoc = pval.intval;

			ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_REPORT_FULL_RAWSOC, &pval);
			if (ret) {
			      report_full_rsoc = 9900;
			      chr_err("failed to get report_full_rsoc\n");
			}
			else
			      report_full_rsoc = pval.intval;

			chr_err("%s: info->tbat = %d, info->rawsoc = %d, report_full_rsoc = %d", __func__, info->tbat, info->rawsoc, report_full_rsoc);
			if (info->rawsoc <= report_full_rsoc +  100)
			      recharge_count++;
			else
			      recharge_count = 0;
		} else {
			if (info->vbat <= info->fv_effective - (product_name == RUBY ? 180 : 200))
			      recharge_count++;
			else
			      recharge_count = 0;
		}

		if (recharge_count >= 5 || info->soc < 100) {
			info->charge_full = false;
			info->recharge = true;
			info->cp_taper = false;
			recharge_count = 0;
			vote(info->bbc_en_votable, FULL_ENABLE_VOTER, true, 1);
			power_supply_changed(info->usb_psy);
			chr_err("%s: start recharge\n", __func__);
		}
	} else {
		recharge_count = 0;
		if (product_name == RUBYPLUS){
			charger_dev_is_enabled(info->cp_master, &cp_master_enable);
			threshold_ma = cp_master_enable ? 100 : 20;
			threshold_mv = info->tbat < 0 ? 30 : 20;
			if(info->cycle_count_status == CYCLE_COUNT_NORMAL){
				threshold_ma += 5;
				threshold_mv += 5;
			}
			else if(info->cycle_count_status == CYCLE_COUNT_HIGH){
				threshold_ma += 10;
				threshold_mv += 10;
			}
		}

		/* dynamic fv pplicy: WA for mp2762 big fv error at low temperature, WA for Vpack Vcell delta */
		if(info->dynamic_fv_hold == 1){
			//Nothing to do
		}else if(product_name == RUBYPLUS && (info->vbat > info->fv_effective - info->fv_ffc_delta)){
			info->dynamic_fv_down_cnt++;
		}else if(product_name != RUBYPLUS && info->ffc_enable && (info->vbat > info->fv_effective - info->fv_ffc_delta)){
			info->dynamic_fv_down_cnt++;
		}else{
			info->dynamic_fv_down_cnt = 0;
		}

		if(info->dynamic_fv_flag == true && info->dynamic_fv_hold != 2 && info->vbat + 40 <= info->fv_effective - info->fv_ffc_delta)
		      info->dynamic_fv_up_cnt++;
		else
		      info->dynamic_fv_up_cnt = 0;

		if(info->dynamic_fv_down_cnt >= 5) {
			fv_result = get_effective_result(info->bbc_fv_votable);
			if(fv_result + (product_name == RUBYPLUS ? 100 : 40) > info->fv_effective){
				vote(info->bbc_fv_votable, DYNAMIC_FV_VOTER, true, fv_result - (product_name == RUBYPLUS ? 25 : 10));
				chr_err("%s: dynamic_fv_down info->vbat = %d, fv_result = %d, info->fv_effective = %d", __func__, info->vbat, fv_result, info->fv_effective);
			}
			info->dynamic_fv_flag = true;
			info->dynamic_fv_hold = 1;
			info->dynamic_fv_down_cnt = 0;
		}
		if(info->dynamic_fv_up_cnt >= 5){
			fv_result = get_effective_result(info->bbc_fv_votable);
			if(fv_result < info->fv_effective){
				vote(info->bbc_fv_votable, DYNAMIC_FV_VOTER, true, fv_result + (product_name == RUBYPLUS ? 25 : 10));
				chr_err("%s: dynamic_fv_up info->vbat = %d, fv_result = %d, info->fv_effective = %d", __func__, info->vbat, fv_result, info->fv_effective);
			}
			else{
				vote(info->bbc_fv_votable, DYNAMIC_FV_VOTER, false, 0);
				info->dynamic_fv_flag = false;
			}
			info->dynamic_fv_hold = 2;
			info->dynamic_fv_up_cnt = 0;
		}

		if(info->dynamic_fv_hold != 0){
			++info->dynamic_fv_hold_cnt;
			if(info->dynamic_fv_hold_cnt > 30){
				info->dynamic_fv_hold_cnt = 0;
				info->dynamic_fv_hold = 0;
			}
			chr_err("%s: dynamic_fv_hold = %d, dynamic_fv_hold_cnt = %d", __func__, info->dynamic_fv_hold, info->dynamic_fv_hold_cnt);
		}

		if (((product_name == RUBYPLUS) && (info->soc == 100) && (-info->ibat <= (info->iterm_effective + threshold_ma)) && (info->vbat >= (info->fv_effective - threshold_mv))) ||
		    (info->charge_status == bbc_full && (-info->ibat <= (info->iterm_effective + threshold_ma))) ||
			(info->fg_full && (-info->ibat <= (iterm + threshold_ma)) && (info->vbat >= info->fv_effective - threshold_mv)) ||
			(product_name == RUBY && info->ffc_enable && (info->vbat >= (4471 - info->diff_fv_val)) && info->fg_full && (-info->ibat <= (iterm + threshold_ma))) ||
			(product_name == RUBYPLUS && info->ffc_enable && info->vbat >= 8930 && (-info->ibat <= (iterm + threshold_ma)))) {
			if (info->soc == 100) {
				full_count++;
				full_wa_count = 0;
			} else if (is_between(94, 99, info->soc)) {
				full_count = 0;
				full_wa_count++;
				if (full_wa_count >= 5) {
					info->full_wa_iterm = min(info->full_wa_iterm + threshold_ma, 1000);
					full_wa_count = 0;
				}
			}
		} else {
			full_count = 0;
		}

		if (full_count >= 4 || (product_name == RUBYPRO && info->cp_taper)) {
			info->charge_full = true;
			info->recharge = false;
			full_count = 0;
			vote(info->bbc_en_votable, FULL_ENABLE_VOTER, true, 0);
			power_supply_changed(info->usb_psy);
			chr_err("%s: report charge_full\n", __func__);
		}
	}
}

static void monitor_thermal_limit(struct charger_manager *info)
{
	int thermal_level = 0;
	union power_supply_propval pval = {0,};
  	power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &pval);
	info->psy_type = pval.intval;

	if (info->thermal_level < 0) {
		thermal_level = -1 - info->thermal_level;
		if (info->thermal_limit_fcc)
			info->thermal_limit[5][thermal_level] = info->thermal_limit_fcc;
	}else
			thermal_level = info->thermal_level;

	if (product_name == RUBY || product_name == RUBYPRO) {
		if (info->last_thermal_level < 15 && thermal_level == 15) {
			chr_info("[CHARGE_LOOP] disable TE\n");
			charger_dev_enable_termination(info->chg1_dev, false);
			vote(info->bbc_iterm_votable, ITERM_WA_VOTER, true, 200);
			msleep(150);
                }
	}
	chr_err("%s: info->psy_type = %d, thermal_level = %d, ", __func__, info->psy_type, thermal_level);
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

	if (product_name == RUBY || product_name == RUBYPRO) {
		if (info->last_thermal_level == 15 && thermal_level < 15) {
			chr_info("[CHARGE_LOOP] enable TE\n");
			msleep(150);
			charger_dev_enable_termination(info->chg1_dev, true);
			vote(info->bbc_iterm_votable, ITERM_WA_VOTER, false, 0);
		}
	}
	info->last_thermal_level = thermal_level;
}

static void monitor_night_charging(struct charger_manager *info)
{
	if (info == NULL || !info->bbc_en_votable)
		return;

	if (info->night_charging && info->soc >=80) {
		vote(info->bbc_en_votable, NIGHT_CHARGING_VOTER, true, 0);
	} else if (!info->night_charging || info->soc <=75) {
		vote(info->bbc_en_votable, NIGHT_CHARGING_VOTER, true, 1);
	}
}

static void quick_update_battery_info(struct charger_manager *info)
{
	union power_supply_propval pval = {0,};
	int ret = 0;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret)
		chr_err("failed to get vbat\n");
	else
		info->vbat = pval.intval / 1000;

	if (info->pd_type != MTK_PD_CONNECT_PE_READY_SNK_APDO || !info->pd_verifed)
	      return;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret)
		chr_err("failed to get ibat\n");
	else
		info->ibat = pval.intval / 1000;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret)
		chr_err("failed to get tbat\n");
	else
		info->tbat = pval.intval;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_RSOC, &pval);
	if (ret)
		chr_err("failed to get rsoc\n");
	else
		info->rsoc = pval.intval;

	if((product_name == RUBYPLUS) && (info->cycle_count_status == CYCLE_COUNT_FRESH)){
		if((-info->ibat >= 13000) && (info->max_power_flag == 0)){
			info->max_power_flag = 1;
			schedule_delayed_work(&info->max_power_work, msecs_to_jiffies(35*1000));
		}

		if((info->rsoc >= 17) && (info->soc_max_power_flag == false)){
			info->soc_max_power_flag = true;
			vote(info->bbc_fcc_votable, SOC_VOTER, true, 15000);
		}
		else if((info->rsoc < 17) && (info->soc_max_power_flag == true)){
			info->soc_max_power_flag = false;
			vote(info->bbc_fcc_votable, SOC_VOTER, false, 0);
		}

	}
}

static void max_power_func(struct work_struct *work)
{
	struct charger_manager *info = container_of(work, struct charger_manager, max_power_work.work);

	if(info->max_power_flag == 1){
		info->max_power_flag = 2;
		vote(info->bbc_fcc_votable, MAX_POWER_VOTER, true, 17000);
		schedule_delayed_work(&info->max_power_work, msecs_to_jiffies(20*1000));
	}else if(info->max_power_flag == 2){
		info->max_power_flag = 3;
		vote(info->bbc_fcc_votable, MAX_POWER_VOTER, true, 15000);
	}
}

static void charge_monitor_func(struct work_struct *work)
{
	struct charger_manager *info = container_of(work, struct charger_manager, charge_monitor_work.work);

	quick_update_battery_info(info);

	handle_ffc_charge(info);

	check_full_recharge(info);

	monitor_thermal_limit(info);

	handle_step_charge(info);

	handle_jeita_charge(info);

	monitor_night_charging(info);

	monitor_sw_cv(info);

	monitor_jeita_descent(info);

	monitor_typec_burn(info);

	chr_err("%s: bat=[%d,%d,%d],step_chg=[%d,%d,%d,%d],sw_cv=[%d,%d],jeita=[%d,%d,%d,%d]",
				__func__, info->vbat,info->ibat,info->tbat,
				info->step_chg_index[0],info->step_chg_index[1],info->step_chg_fcc,info->step_chg_fv,
				info->sw_cv,info->sw_cv_count,
				info->jeita_chg_index[0],info->jeita_chg_index[1],info->jeita_chg_fcc,info->psy_type);

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

	if (info->cycle_count_status == CYCLE_COUNT_FRESH) {
		total_length = of_property_count_elems_of_size(np, "step_chg_cfg_fresh_cycle", sizeof(u32));
		if (total_length < 0) {
			chr_err("failed to read total_length of config\n");
			return -EINVAL;
		}
		ret = of_property_read_u32_array(np, "step_chg_cfg_fresh_cycle", (u32 *)info->step_chg_cfg, total_length);
		if (ret) {
			chr_err("failed to parse step_chg_cfg_fresh_cycle\n");
			return ret;
		}
	} else if (info->cycle_count_status == CYCLE_COUNT_LOW) {
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

	if (product_name != RUBYPLUS) {
		if (info->cycle_count <= 100) {
			if (info->cycle_count_status != CYCLE_COUNT_LOW) {
				info->cycle_count_status = CYCLE_COUNT_LOW;
				update = true;
			}
		} else {
			if (info->cycle_count_status != CYCLE_COUNT_HIGH) {
				info->cycle_count_status = CYCLE_COUNT_HIGH;
				update = true;
			}
		}
	} else { 
		if (info->cycle_count <= 50) {
			if (info->cycle_count_status != CYCLE_COUNT_FRESH) {
				info->cycle_count_status = CYCLE_COUNT_FRESH;
				update = true;
			}
		} else if (info->cycle_count <= 100) {
			if (info->cycle_count_status != CYCLE_COUNT_LOW) {
				info->cycle_count_status = CYCLE_COUNT_LOW;
				update = true;
			}
		} else if (info->cycle_count <= 200) {
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
	}

	if (update || farce_update)
		parse_cycle_count_step_chg_cfg(info);

	return;
}

void reset_mi_charge_alg(struct charger_manager *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->step_jeita_tuple_count, info->vbat, info->step_chg_index, true);
	info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;
	vote(info->bbc_fcc_votable, STEP_CHARGE_VOTER, true, info->step_chg_fcc);

	/*The high temperature drops to 48 degrees to stop charging, need to remove compensation*/
	if (info->tbat > 460) {
		info->jeita_fallback_hyst = 9;
		info->jeita_forward_hyst = 9;
	} else {
		info->jeita_fallback_hyst = 5;
		info->jeita_forward_hyst = 5;
	}

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->step_jeita_tuple_count, info->tbat, info->jeita_chg_index, true);
	if (info->vbat < info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold)
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
	else
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
	vote(info->bbc_fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);

	if (info->charger_status == CHARGER_PLUGOUT)
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

		check_cycle_count_status(info, true);

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

	info->sic_support =  of_property_read_bool(np, "sic_support");
	if (!info->sic_support)
		chr_err("failed to parse sic_support\n");

	ret = of_property_read_u32(np, "step_jeita_tuple_count", &info->step_jeita_tuple_count);
	if (ret) {
		chr_err("failed to parse step_jeita_tuple_count\n");
		return ret;
	}

	INIT_DELAYED_WORK(&info->charge_monitor_work, charge_monitor_func);
	INIT_DELAYED_WORK(&info->usb_otg_monitor_work, monitor_usb_otg_burn);
	if (product_name == RUBYPLUS)
	      INIT_DELAYED_WORK(&info->max_power_work, max_power_func);

	return ret;
}
