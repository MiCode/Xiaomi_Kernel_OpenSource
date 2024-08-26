// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Xiaomi Inc.
 * Author Tianye<tianye9@xiaomi.com>
 */
#include <linux/delay.h>
#include "xm_smart_chg.h"

#define XM_SMART_CHG_DEBUG 0

void set_error(struct charger_manager *manager)
{
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 1;
	dev_err(manager->dev,"xm %s en_ret=%d\n", manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

void set_success(struct charger_manager *manager)
{
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 0;
	dev_err(manager->dev, "xm %s en_ret=%d\n", manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

int smart_chg_is_error(struct charger_manager *manager)
{
	return manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret? true : false;
}

void handle_smart_chg_functype(struct charger_manager *manager,
	const int func_type, const int en_ret, const int func_val)
{
	switch (func_type)
	{
	case SMART_CHG_FEATURE_MIN_NUM ... SMART_CHG_FEATURE_MAX_NUM:
		manager->smart_charge[func_type].en_ret = en_ret;
		manager->smart_charge[func_type].active_status = false;
		manager->smart_charge[func_type].func_val = func_val;
		set_success(manager);
		dev_err(manager->dev, "xm set func_type:%d, en_ret = %d\n", func_type, en_ret);
		break;
	default:
		dev_err(manager->dev, "xm ERROR: Not supported func type: %d\n", func_type);
		set_error(manager);
		break;
	}
}

int handle_smart_chg_functype_status(struct charger_manager *manager)
{
	int i;
	int all_func_status = 0;
	all_func_status |= !!manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret;	//handle bit0

	dev_err(manager->dev, "smart_chg: all_func_status =%#X, en_ret=%d\n",all_func_status, manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);

	/* save functype[i] enable status in all_func_status bit[i] */
	for(i = SMART_CHG_FEATURE_MIN_NUM; i <= SMART_CHG_FEATURE_MAX_NUM; i++){  //handle bit1 ~ bit SMART_CHG_FEATURE_MAX_NUM
		if(manager->smart_charge[i].en_ret)
			all_func_status |= BIT_MASK(i);
		else
			all_func_status &= ~BIT_MASK(i);

		dev_err(manager->dev, "smart_chg: type:%d, en_ret=%d, active_status=%d,func_val=%d, all_func_status=%#X\n",
			i, manager->smart_charge[i].en_ret, manager->smart_charge[i].active_status, manager->smart_charge[i].func_val,all_func_status);
	}
	dev_err(manager->dev, "smart_chg: all_func_status:%#X\n", all_func_status);
	return all_func_status;
}

void monitor_smart_chg(struct charger_manager *manager)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
        struct votable		*fcc_votable;

        fcc_votable = find_votable("TOTAL_FCC");
        if (!fcc_votable) {
                pr_err("%s failed to get fcc_votable\n", __func__);
                return;
        }

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		dev_err(manager->dev, "get battery soc error.\n");
	else
		manager->soc = pval.intval;

	if (g_policy == NULL)
		return;
        dev_err(manager->dev, "SMART_CHG: ENDURANCE en_ret = %d, fun_val = %d, active_status = %d, endurance_ctrl_en = %d, ui_soc =%d, state = %d\n",
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret,
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val,
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status,
                manager->endurance_ctrl_en, manager->soc, g_policy->state);

        if((manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && manager->soc >= manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) ||
        ((manager->soc > (manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val - 5)) &&
        (manager->soc < manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) &&
        manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status))
	{
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = true;
		vote(fcc_votable, ENDURANCE_VOTER, true, 0);
                rerun_election(fcc_votable);
		manager->endurance_ctrl_en = true;
		dev_err(manager->dev, "SMART_CHG: ENDURANCE disable charger, uisoc(%d) fuc_val(%d)\n", manager->soc, manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val);
	}
	else if((((!manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret || manager->soc <= (manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val - 5)) && manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status) ||
		(!manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && !manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status)))
	{
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
		vote(fcc_votable, ENDURANCE_VOTER, false, 0);
                rerun_election(fcc_votable);
		manager->endurance_ctrl_en = false;
                if(manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret){
		        dev_err(manager->dev, "SMART_CHG: ENDURANCE enable charger, uisoc(%d) fuc_val(%d)\n", manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val);
                }
	}

        if(manager->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
                manager->smart_charge[SMART_CHG_LOW_FAST].active_status = true;
                dev_err(manager->dev, "N19A set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, enable low_fast\n", manager->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
	else if(!manager->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
		manager->smart_charge[SMART_CHG_LOW_FAST].active_status = false;
                dev_err(manager->dev, "N19A set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, disable low_fast\n", manager->smart_charge[SMART_CHG_LOW_FAST].en_ret);
        }

        if(manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret)
	{
                manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].active_status = true;
                dev_err(manager->dev, "N19A set smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret = %d, enable outdoor_charge\n", manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret);
	}
	else if(!manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret)
	{
		manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].active_status = false;
                dev_err(manager->dev, "N19A set smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret = %d, disable outdoor_charge\n", manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret);
	}
}

void get_fv_againg(struct charger_manager *manager, int cyclecount, int *fv_aging)
{
        int i = 0;

        while (cyclecount > manager->cyclecount[i]) {
                i++;
                if (i == 3)
                        break;
        }

        if(manager->tbat >= 150 && manager->tbat <= 450) {
                if (manager->pd_adapter->verifed && (g_policy->sm == PM_STATE_CHARGERPUMP_CC_CV || g_policy->cp_charge_done)) {
                        *fv_aging = manager->dropfv[i];
                } else {
                        *fv_aging = manager->dropfv_normal[i];
                }
        } else {
                *fv_aging = 0;
        }

        pr_info("%s i = %d, fv_aging = %d\n", __func__, i, *fv_aging);
        return;
}

void get_drop_floatvolatge(struct charger_manager *manager)
{
        int ret = 0;
        union power_supply_propval pval;
        static last_fv_againg = 0;
        struct votable	*fv_votable = NULL;

	if (IS_ERR_OR_NULL(manager->fg_psy)) {
		manager->fg_psy = power_supply_get_by_name("bms");
		if (IS_ERR_OR_NULL(manager->fg_psy))
			return;
	}

        ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
        if (ret < 0) {
                pr_err("%s failed to get cycle_count prop", __func__);
                return;
        }
        manager->batt_cycle = pval.intval;

        get_fv_againg(manager, manager->batt_cycle, &manager->fv_againg);
        if(manager->fv_againg != last_fv_againg){
                fv_votable = find_votable("MAIN_FV");
                if (!fv_votable) {
                        pr_err("%s failed to get fv_votable\n", __func__);
                }else{
                        rerun_election(fv_votable);
                }
        }
        last_fv_againg = manager->fv_againg;
}

void monitor_night_charging(struct charger_manager *manager)
{
        if ((manager == NULL) || !manager->main_chg_disable_votable || !manager->cp_disable_votable)
		return;

        pr_debug("%s night_charging = %d, soc = %d\n", __func__, manager->night_charging, manager->soc);
        if (manager->night_charging && (manager->soc >= 80)) {
                manager->night_charging_flag = true;
                pr_err("%s disable charging\n", __func__);
                charger_set_chg(manager->charger, false);
		if (g_policy->state == POLICY_RUNNING){
			chargerpump_policy_stop(g_policy);
			dev_err(manager->dev, "%s N19A monitor disable cp\n", __func__);
		}
	} else if(manager->night_charging_flag && (!manager->night_charging || manager->soc <=75)) {
                manager->night_charging_flag = false;
                pr_err("%s enable charging\n", __func__);
		charger_set_chg(manager->charger, true);
		if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START)){
			chargerpump_policy_start(g_policy);
			dev_err(manager->dev, "%s N19A monitor enable cp\n", __func__);
		}
	}
}

void monitor_low_fast_strategy(struct charger_manager *manager)
{
        bool fast_flag = false;
        time64_t time_now = 0, delta_time = 0;
        static time64_t time_last = 0;
        static int last_level = 0;
        static bool hot_flag = false;

        if (manager == NULL)
		return;
        if (manager->system_temp_level <= 0)
		goto err;

        pr_err("%s soc = %d, thermal_level = %d, thermal_board_temp = %d, pd_active = %d, low_fast_plugin_flag = %d, low_fast_enable = %d, screen_state = %d, b_flag = %d\n", 
                __func__, manager->soc, manager->system_temp_level, manager->thermal_board_temp, manager->pd_active, manager->low_fast_plugin_flag, manager->smart_charge[SMART_CHG_LOW_FAST].active_status, 
                manager->sm.screen_state, manager->b_flag);

        if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (manager->soc <= 40) && (manager->low_fast_plugin_flag) && manager->smart_charge[SMART_CHG_LOW_FAST].active_status) {
		if (manager->thermal_parse_flags & PD_THERM_PARSE_ERROR) {
			pr_err("%s: pd thermal dtsi parse error\n", __func__);
			goto err;
		}
		if (manager->system_temp_level > manager->pd_thermal_levels) {
			pr_err("%s: system_temp_level is invalid\n", __func__);
			goto err;
		}

                /*manager->sm.screen_state 0:bright, 1:black*/
                if(((manager->b_flag == NORMAL) || (manager->b_flag == BLACK)) && !manager->sm.screen_state) {  //black to bright
                        manager->b_flag = BLACK_TO_BRIGHT;
                        time_last = ktime_get_seconds();
                        fast_flag = true;
                        pr_err("%s switch to bright time_last = %d\n", __func__, time_last);
                }
                else if((manager->b_flag == BLACK_TO_BRIGHT || manager->b_flag == BRIGHT) && !manager->sm.screen_state) {  //still bright
                        manager->b_flag = BRIGHT;
                        time_now = ktime_get_seconds();
                        delta_time = time_now - time_last;
                        pr_err("%s still_bright time_now = %d, time_last = %d, delta_time = %d\n", __func__, time_now, time_last, delta_time);
                        if(delta_time <= 10) {
                                fast_flag = true;
                                pr_err("%s still_bright delta_time = %d, stay fast\n", __func__, delta_time);
                        }
                        else {
                                fast_flag = false;
                                pr_err("%s still_bright delta_time = %d, exit fast\n", __func__, delta_time);
                        }
                }
                else { //black
                        manager->b_flag = BLACK;
                        fast_flag = true;
                        pr_err("%s black stay fast\n", __func__, delta_time);
                }

                /*avoid thermal_board_temp raise too fast*/
                if((last_level == 8) && (manager->system_temp_level == 7) && (manager->thermal_board_temp > 410)){
                        hot_flag = true;
                        fast_flag = false;
                        pr_err("%s avoid thermal_board_temp raise too fast, exit fast mode\n", __func__);
                }
                else if((last_level == 7) && ((manager->system_temp_level == 7) || (manager->system_temp_level == 8)) && hot_flag && (manager->thermal_board_temp > 410)){
                        fast_flag = false;
                }
                else{
                        hot_flag = false;
                }

                if((manager->thermal_board_temp > 420)){
                        fast_flag = false;
                }

                if(fast_flag) {  //stay fast strategy
                        manager->pps_fast_mode = true;
                        manager->low_fast_ffc = manager->pd_thermal_mitigation_fast[manager->system_temp_level];

                        if((manager->soc > 38) && (manager->thermal_board_temp > 380)){
                                if(manager->low_fast_ffc >= 5400){
                                        manager->low_fast_ffc -= 1900;
                                } else {
                                         manager->low_fast_ffc = 3500;
                                }
                                pr_err("%s stay fast but cool down, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc > 30) && (manager->thermal_board_temp > 400)){
                                if(manager->low_fast_ffc >= 3500){
                                         manager->low_fast_ffc = 3500;
                                }
                                pr_err("%s stay fast but decrease 3.3, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
                        vote(manager->total_fcc_votable, TEMP_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
                        pr_err("%s stay fast, manager->low_fast_ffc = %d\n", __func__, manager->low_fast_ffc);
                }
                else { //exit fast strategy
                        manager->pps_fast_mode = false;
                        manager->low_fast_ffc = manager->pd_thermal_mitigation[manager->system_temp_level];
                        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
                        vote(manager->total_fcc_votable, TEMP_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
                        pr_err("%s exit fast, manager->low_fast_ffc = %d\n", __func__, manager->low_fast_ffc);
                }
                last_level = manager->system_temp_level;
	}

	return;
err:
        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->pd_thermal_mitigation[manager->system_temp_level]);
        vote(manager->total_fcc_votable, TEMP_THERMAL_DAEMON_VOTER, true, manager->pd_thermal_mitigation[manager->system_temp_level]);
        last_level = manager->system_temp_level;
	return;
}

void xm_charge_work(struct work_struct *work)
{
	struct charger_manager *chip = container_of(work, struct charger_manager, xm_charge_work.work);
        if (chip == NULL)
                return;
	dev_err(chip->dev, "N19A:check xm_charge_work\n");
	monitor_smart_chg(chip);
        monitor_night_charging(chip);
        monitor_low_fast_strategy(chip);
        get_drop_floatvolatge(chip);
        /* *
         * Move to hq_charger_manager fv_votable callback
         * monitor_smart_batt(chip);
         * monitor_cycle_count(chip);
         * */

	schedule_delayed_work(&chip->xm_charge_work, msecs_to_jiffies(1000));
}
